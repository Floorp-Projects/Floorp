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

#include "nsImageLoader.h"

#include "imgIRequest.h"

#include "nsIServiceManager.h"

#include "nsIChannel.h"
#include "nsIIOService.h"
#include "nsIRunnable.h"
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

static NS_DEFINE_CID(kImageRequestCID, NS_IMAGEREQUEST_CID);
static NS_DEFINE_CID(kImageRequestProxyCID, NS_IMAGEREQUESTPROXY_CID);

#ifdef LOADER_THREADSAFE
NS_IMPL_THREADSAFE_ISUPPORTS1(nsImageLoader, nsIImageLoader)
#else
NS_IMPL_ISUPPORTS1(nsImageLoader, nsIImageLoader)
#endif

nsImageLoader::nsImageLoader()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
#ifdef LOADER_THREADSAFE
  mLock = PR_NewLock();
#endif
}

nsImageLoader::~nsImageLoader()
{
  /* destructor code */
#ifdef LOADER_THREADSAFE
  PR_DestroyLock(mLock);
#endif
}

/* imgIRequest loadImage (in nsIURI uri, in nsIImageDecoderObserver aObserver, in nsISupports cx); */
NS_IMETHODIMP nsImageLoader::LoadImage(nsIURI *aURI, nsIImageDecoderObserver *aObserver, nsISupports *cx, imgIRequest **_retval)
{
  NS_ASSERTION(aURI, "nsImageLoader::LoadImage -- NULL URI pointer");

  imgRequest *imgRequest = nsnull;

  ImageCache::Get(aURI, &imgRequest); // addrefs
  if (!imgRequest) {
#ifdef LOADER_THREADSAFE
    nsAutoLock lock(mLock); // lock when we are adding things to the cache
#endif
    nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
    if (!ioserv) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIChannel> newChannel;
    ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
    if (!newChannel) return NS_ERROR_FAILURE;

    // XXX do we need to SetOwner here?
    newChannel->SetOwner(this); // the channel is now holding a strong ref to 'this'

    nsCOMPtr<imgIRequest> req(do_CreateInstance(kImageRequestCID));
    imgRequest = NS_REINTERPRET_CAST(imgRequest*, req.get());
    NS_ADDREF(imgRequest);

    imgRequest->Init(newChannel);

    ImageCache::Put(aURI, imgRequest);

    newChannel->AsyncRead(NS_STATIC_CAST(nsIStreamListener *, imgRequest), cx);  // XXX are we calling this too early?
  }

  nsCOMPtr<imgIRequest> proxyRequest(do_CreateInstance(kImageRequestProxyCID));
  // init adds itself to imgRequest's list of observers
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(imgRequest, aObserver, cx);

  NS_RELEASE(imgRequest);

  *_retval = proxyRequest;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* imgIRequest loadImageWithChannel(in nsIChannel, in nsIImageDecoderObserver aObserver, in nsISupports cx, out nsIStreamListener); */
NS_IMETHODIMP nsImageLoader::LoadImageWithChannel(nsIChannel *channel, nsIImageDecoderObserver *aObserver, nsISupports *cx, nsIStreamListener **listener, imgIRequest **_retval)
{
  NS_ASSERTION(channel, "nsImageLoader::LoadImageWithChannel -- NULL channel pointer");

  imgRequest *imgRequest = nsnull;

  nsCOMPtr<nsIURI> uri;
  channel->GetURI(getter_AddRefs(uri));

  ImageCache::Get(uri, &imgRequest);
  if (imgRequest) {
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

    imgRequest = NS_REINTERPRET_CAST(imgRequest*, req.get());
    NS_ADDREF(imgRequest);

    imgRequest->Init(channel);

    ImageCache::Put(uri, imgRequest);

    *listener = NS_STATIC_CAST(nsIStreamListener*, imgRequest);
    NS_IF_ADDREF(*listener);
  }

  nsCOMPtr<imgIRequest> proxyRequest(do_CreateInstance(kImageRequestProxyCID));

  // init adds itself to imgRequest's list of observers
  NS_REINTERPRET_CAST(imgRequestProxy*, proxyRequest.get())->Init(imgRequest, aObserver, cx);

  NS_RELEASE(imgRequest);

  *_retval = proxyRequest;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator requests; */
NS_IMETHODIMP nsImageLoader::GetRequests(nsISimpleEnumerator * *aRequests)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

