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

#include "imgIRequest.h"

#include "nsIServiceManager.h"

#include "nsIChannel.h"
#include "nsIIOService.h"
#include "nsILoadGroup.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"

#include "imgRequest.h"
#include "imgRequestProxy.h"

#include "ImageCache.h"

#include "nsXPIDLString.h"

#include "nsCOMPtr.h"

#ifdef LOADER_THREADSAFE
#include "nsAutoLock.h"
#endif

#include "ImageLogging.h"

static NS_DEFINE_CID(kImageRequestCID, NS_IMGREQUEST_CID);
static NS_DEFINE_CID(kImageRequestProxyCID, NS_IMGREQUESTPROXY_CID);

#ifdef LOADER_THREADSAFE
NS_IMPL_THREADSAFE_ISUPPORTS1(imgLoader, imgILoader)
#else
NS_IMPL_ISUPPORTS1(imgLoader, imgILoader)
#endif

imgLoader::imgLoader()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
#ifdef LOADER_THREADSAFE
  mLock = PR_NewLock();
#endif
}

imgLoader::~imgLoader()
{
  /* destructor code */
#ifdef LOADER_THREADSAFE
  PR_DestroyLock(mLock);
#endif
}

/* imgIRequest loadImage (in nsIURI uri, in nsILoadGroup aLoadGroup, in imgIDecoderObserver aObserver, in nsISupports cx); */
NS_IMETHODIMP imgLoader::LoadImage(nsIURI *aURI, nsILoadGroup *aLoadGroup, imgIDecoderObserver *aObserver, nsISupports *cx, imgIRequest **_retval)
{
#if defined(PR_LOGGING)
  nsXPIDLCString spec;
  aURI->GetSpec(getter_Copies(spec));
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgLoader::LoadImage", "aURI", spec.get());
#endif

  NS_ASSERTION(aURI, "imgLoader::LoadImage -- NULL URI pointer");

  imgRequest *request = nsnull;

#ifdef MOZ_NEW_CACHE
  nsCOMPtr<nsICacheEntryDescriptor> entry;
  ImageCache::Get(aURI, &request, getter_AddRefs(entry)); // addrefs request

  if (request && entry && aLoadGroup) {
    /* this isn't exactly what I want here.  This code will re-doom every cache hit in a document while
       it is force reloading.  So for multiple copies of an image on a page, when you force reload, this
       will cause you to get seperate loads for each copy of the image... this sucks.
     */
    PRUint32 flags = 0;
    PRBool doomRequest = PR_FALSE;
    aLoadGroup->GetDefaultLoadAttributes(&flags);
    if (flags & nsIChannel::FORCE_RELOAD)
      doomRequest = PR_TRUE;
    else {
      nsCOMPtr<nsIRequest> r;
      aLoadGroup->GetDefaultLoadRequest(getter_AddRefs(r));
      if (r) {
        nsCOMPtr<nsIChannel> c(do_QueryInterface(r));
        if (c) {
          c->GetLoadAttributes(&flags);
          if (flags & nsIChannel::FORCE_RELOAD)
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
#endif

  if (!request) {
#ifdef LOADER_THREADSAFE
    nsAutoLock lock(mLock); // lock when we are adding things to the cache
#endif
    LOG_SCOPE(gImgLog, "imgLoader::LoadImage |cache miss|");

    nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
    if (!ioserv) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIChannel> newChannel;
    ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
    if (!newChannel) return NS_ERROR_FAILURE;

    if (aLoadGroup) {
      PRUint32 flags;
      aLoadGroup->GetDefaultLoadAttributes(&flags);
      newChannel->SetLoadAttributes(flags);
    }

    nsCOMPtr<imgIRequest> req(do_CreateInstance(kImageRequestCID));
    request = NS_REINTERPRET_CAST(imgRequest*, req.get());
    NS_ADDREF(request);

    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Created new imgRequest [request=%p]\n", this, request));

    request->Init(newChannel);

#ifdef MOZ_NEW_CACHE
    ImageCache::Put(aURI, request, getter_AddRefs(entry));
#endif
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage -- Calling channel->AsyncOpen()\n", this));

    // XXX are we calling this too early?
    newChannel->AsyncOpen(NS_STATIC_CAST(nsIStreamListener *, request), nsnull);

  } else {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgLoader::LoadImage |cache hit| [request=%p]\n",
            this, request));
  }

  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgLoader::LoadImage -- creating proxy request.\n", this));

  nsCOMPtr<imgIRequest> proxyRequest(do_CreateInstance(kImageRequestProxyCID));
  // init adds itself to imgRequest's list of observers
#ifdef MOZ_NEW_CACHE
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(request, aLoadGroup, aObserver, cx, entry);
#else
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(request, aLoadGroup, aObserver, cx, nsnull);
#endif
  NS_RELEASE(request);

  *_retval = proxyRequest;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* imgIRequest loadImageWithChannel(in nsIChannel, in imgIDecoderObserver aObserver, in nsISupports cx, out nsIStreamListener); */
NS_IMETHODIMP imgLoader::LoadImageWithChannel(nsIChannel *channel, imgIDecoderObserver *aObserver, nsISupports *cx, nsIStreamListener **listener, imgIRequest **_retval)
{
  NS_ASSERTION(channel, "imgLoader::LoadImageWithChannel -- NULL channel pointer");

  imgRequest *request = nsnull;

  nsCOMPtr<nsIURI> uri;
  channel->GetOriginalURI(getter_AddRefs(uri));

#ifdef MOZ_NEW_CACHE
  nsCOMPtr<nsICacheEntryDescriptor> entry;
  ImageCache::Get(uri, &request, getter_AddRefs(entry)); // addrefs request
#endif
  if (request) {
    // we have this in our cache already.. cancel the current (document) load

    // XXX
    // if *listener is null when we return here, the caller should probably cancel
    // the channel instead of us doing it here.
    channel->Cancel(NS_BINDING_ABORTED); // this should fire an OnStopRequest

    *listener = nsnull; // give them back a null nsIStreamListener
  } else {
#ifdef LOADER_THREADSAFE
    nsAutoLock lock(mLock); // lock when we are adding things to the cache
#endif

    nsCOMPtr<imgIRequest> req(do_CreateInstance(kImageRequestCID));

    request = NS_REINTERPRET_CAST(imgRequest*, req.get());
    NS_ADDREF(request);

    request->Init(channel);

#ifdef MOZ_NEW_CACHE
    ImageCache::Put(uri, request, getter_AddRefs(entry));
#endif

    *listener = NS_STATIC_CAST(nsIStreamListener*, request);
    NS_IF_ADDREF(*listener);
  }

  nsCOMPtr<imgIRequest> proxyRequest(do_CreateInstance(kImageRequestProxyCID));

  // init adds itself to imgRequest's list of observers
#ifdef MOZ_NEW_CACHE
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(request, nsnull, aObserver, cx, entry);
#else
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(request, nsnull, aObserver, cx, nsnull);
#endif
  NS_RELEASE(request);

  *_retval = proxyRequest;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator requests; */
NS_IMETHODIMP imgLoader::GetRequests(nsISimpleEnumerator * *aRequests)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

