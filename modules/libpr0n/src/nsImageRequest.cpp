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

#include "nsImageRequest.h"

#include "nsXPIDLString.h"

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "nsIImageLoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsString.h"

#include "ImageCache.h"


NS_IMPL_ISUPPORTS5(nsImageRequest, lpIImageRequest, 
                   nsIImageDecoderObserver, nsIImageContainerObserver,
                   nsIStreamListener, nsIStreamObserver)

nsImageRequest::nsImageRequest() : 
  mObservers(0), mProcessing(PR_TRUE), mStatus(lpIImageRequest::STATUS_NONE), mState(0)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsImageRequest::~nsImageRequest()
{
  /* destructor code */
}


nsresult nsImageRequest::Init(nsIChannel *aChannel)
{
  // XXX we should save off the thread we are getting called on here so that we can proxy all calls to mDecoder to it.

  NS_ASSERTION(!mImage, "nsImageRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "nsImageRequest::Init -- No channel");

  mChannel = aChannel;

  // XXX do not init the image here.  this has to be done from the image decoder.
  mImage = do_CreateInstance("@mozilla.org/gfx/image;2");

  return NS_OK;
}

nsresult nsImageRequest::AddObserver(nsIImageDecoderObserver *observer)
{
  mObservers.AppendElement(NS_STATIC_CAST(void*, observer));

  if (mState & onStartDecode)
    observer->OnStartDecode(nsnull, nsnull);
  if (mState & onStartContainer)
    observer->OnStartContainer(nsnull, nsnull, mImage);

  // XXX send the decoded rect in here

  if (mState & onStopContainer)
    observer->OnStopContainer(nsnull, nsnull, mImage);
  if (mState & onStopDecode)
    observer->OnStopDecode(nsnull, nsnull, NS_OK, nsnull);

  return NS_OK;
}

nsresult nsImageRequest::RemoveObserver(nsIImageDecoderObserver *observer, nsresult status)
{
  mObservers.RemoveElement(NS_STATIC_CAST(void*, observer));

  if ((mObservers.Count() == 0) && mChannel && mProcessing) {
    mChannel->Cancel(status);
  }
  return NS_OK;
}



/** lpIImageRequest methods **/


/* void cancel (in nsresult status); */
NS_IMETHODIMP nsImageRequest::Cancel(nsresult status)
{
  // XXX this method should not ever get called

  nsresult rv = NS_OK;
  if (mChannel && mProcessing) {
    rv = mChannel->Cancel(status);
  }
  return rv;
}

/* readonly attribute nsIImage image; */
NS_IMETHODIMP nsImageRequest::GetImage(nsIImageContainer * *aImage)
{
  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP nsImageRequest::GetImageStatus(PRUint32 *aStatus)
{
  *aStatus = mStatus;
  return NS_OK;
}




/** nsIImageContainerObserver methods **/

/* [noscript] void frameChanged (in nsIImageContainer container, in nsISupports cx, in nsIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP nsImageRequest::FrameChanged(nsIImageContainer *container, nsISupports *cx, nsIImageFrame *newframe, nsRect * dirtyRect)
{
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->FrameChanged(container, cx, newframe, dirtyRect);
  }

  return NS_OK;
}

/** nsIImageDecoderObserver methods **/

/* void onStartDecode (in lpIImageRequest request, in nsISupports cx); */
NS_IMETHODIMP nsImageRequest::OnStartDecode(lpIImageRequest *request, nsISupports *cx)
{
  mState |= onStartDecode;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartDecode(request, cx);
  }

  return NS_OK;
}

/* void onStartContainer (in lpIImageRequest request, in nsISupports cx, in nsIImageContainer image); */
NS_IMETHODIMP nsImageRequest::OnStartContainer(lpIImageRequest *request, nsISupports *cx, nsIImageContainer *image)
{
  mState |= onStartContainer;

  mStatus |= lpIImageRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStartFrame (in lpIImageRequest request, in nsISupports cx, in nsIImageFrame frame); */
NS_IMETHODIMP nsImageRequest::OnStartFrame(lpIImageRequest *request, nsISupports *cx, nsIImageFrame *frame)
{
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartFrame(request, cx, frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in lpIImageRequest request, in nsISupports cx, in nsIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP nsImageRequest::OnDataAvailable(lpIImageRequest *request, nsISupports *cx, nsIImageFrame *frame, const nsRect * rect)
{
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnDataAvailable(request, cx, frame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in lpIImageRequest request, in nsISupports cx, in nsIImageFrame frame); */
NS_IMETHODIMP nsImageRequest::OnStopFrame(lpIImageRequest *request, nsISupports *cx, nsIImageFrame *frame)
{
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopFrame(request, cx, frame);
  }

  return NS_OK;
}

/* void onStopContainer (in lpIImageRequest request, in nsISupports cx, in nsIImageContainer image); */
NS_IMETHODIMP nsImageRequest::OnStopContainer(lpIImageRequest *request, nsISupports *cx, nsIImageContainer *image)
{
  mState |= onStopContainer;


  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStopDecode (in lpIImageRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP nsImageRequest::OnStopDecode(lpIImageRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  mState |= onStopDecode;

  if (NS_FAILED(status))
    mStatus = lpIImageRequest::STATUS_ERROR;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    nsIImageDecoderObserver *ob = NS_STATIC_CAST(nsIImageDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopDecode(request, cx, status, statusArg);
  }

  return NS_OK;
}






/** nsIStreamObserver methods **/

/* void onStartRequest (in nsIChannel channel, in nsISupports ctxt); */
NS_IMETHODIMP nsImageRequest::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
  NS_ASSERTION(!mDecoder, "nsImageRequest::OnStartRequest -- we already have a decoder");

  nsXPIDLCString contentType;
  channel->GetContentType(getter_Copies(contentType));
  printf("content type is %s\n", contentType.get());

  nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
  conid += contentType.get();

  mDecoder = do_CreateInstance(conid);

  if (!mDecoder) {
    // no image decoder for this mimetype :(
    channel->Cancel(NS_BINDING_ABORTED);

    // XXX notify the person that owns us now that wants the nsIImageContainer off of us?

    return NS_ERROR_FAILURE;
  }

  mDecoder->Init(NS_STATIC_CAST(lpIImageRequest*, this));
  return NS_OK;
}

/* void onStopRequest (in nsIChannel channel, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP nsImageRequest::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
  NS_ASSERTION(mChannel || mProcessing, "nsImageRequest::OnStopRequest -- received multiple OnStopRequest");

  mProcessing = PR_FALSE;

  // if we failed, we should remove ourself from the cache
  if (NS_FAILED(status)) {
    nsCOMPtr<nsIURI> uri;
    channel->GetURI(getter_AddRefs(uri));
    ImageCache::Remove(uri);
  }

  mChannel = nsnull; // we no longer need the channel

  if (!mDecoder) return NS_ERROR_FAILURE;

  mDecoder->Close();

  mDecoder = nsnull; // release the decoder so that it can rest peacefully ;)

  return NS_OK;
}




/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIChannel channel, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP nsImageRequest::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  if (!mDecoder) {
    NS_ASSERTION(mDecoder, "nsImageRequest::OnDataAvailable -- no decoder");
    return NS_ERROR_FAILURE;
  }

  PRUint32 wrote;
  return mDecoder->WriteFrom(inStr, count, &wrote);
}
