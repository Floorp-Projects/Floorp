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

#include "nsIImageRequest.h"

#include "nsIServiceManager.h"

#include "nsIChannel.h"
#include "nsIIOService.h"
#include "nsIRunnable.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"

NS_IMPL_ISUPPORTS1(nsImageLoader, nsIImageLoader)

nsImageLoader::nsImageLoader()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageLoader::~nsImageLoader()
{
  /* destructor code */
}

/* nsIImageRequest loadImage (in nsIURI uri, in gfx_dimension width, in gfx_dimension height); */
NS_IMETHODIMP nsImageLoader::LoadImage(nsIURI *aURI, nsIImageRequest **_retval)
{

#ifdef IMAGE_THREADPOOL
  if (!mThreadPool) {
    NS_NewThreadPool(getter_AddRefs(mThreadPool),
                     1, 4,
                     512,
                     PR_PRIORITY_NORMAL,
                     PR_GLOBAL_THREAD);
  }
#endif

/* do we need a loadgroup of channels here?  what does that buy us?
  if (!mLoadGroup)
    NS_NewLoadGroup
*/

  nsCOMPtr<nsIIOService> ioserv(do_GetService("@mozilla.org/network/io-service;1"));
  if (!ioserv) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIChannel> newChannel;
  ioserv->NewChannelFromURI(aURI, getter_AddRefs(newChannel));
  if (!newChannel) return NS_ERROR_FAILURE;

  newChannel->SetOwner(this); // the channel is now holding a strong ref to 'this'

  // XXX look at the progid
  nsCOMPtr<nsIImageRequest> imgRequest(do_CreateInstance("@mozilla.org/image/request;1"));
  imgRequest->Init(newChannel);

#ifdef IMAGE_THREADPOOL
  nsCOMPtr<nsIRunnable> run(do_QueryInterface(imgRequest));
  mThreadPool->DispatchRequest(run);
#else
  nsCOMPtr<nsIStreamListener> streamList(do_QueryInterface(imgRequest));
  newChannel->AsyncRead(streamList, nsnull);
#endif

  *_retval = imgRequest;
  NS_ADDREF(*_retval);

  return NS_OK;
}

/* readonly attribute nsISimpleEnumerator requests; */
NS_IMETHODIMP nsImageLoader::GetRequests(nsISimpleEnumerator * *aRequests)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

