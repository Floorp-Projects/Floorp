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

#include "imgRequest.h"

#include "nsXPIDLString.h"

#include "nsIAtom.h"
#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIHTTPChannel.h"
#include "nsIInputStream.h"
#include "imgILoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsString.h"

#include "ImageCache.h"

#include "ImageLogging.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif


NS_IMPL_ISUPPORTS6(imgRequest, imgIRequest, nsIRequest,
                   imgIDecoderObserver, gfxIImageContainerObserver,
                   nsIStreamListener, nsIStreamObserver)

imgRequest::imgRequest() : 
  mObservers(0), mProcessing(PR_TRUE), mStatus(imgIRequest::STATUS_NONE), mState(0)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgRequest::~imgRequest()
{
  /* destructor code */
}


nsresult imgRequest::Init(nsIChannel *aChannel)
{
  // XXX we should save off the thread we are getting called on here so that we can proxy all calls to mDecoder to it.

  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::Init\n", this));

  NS_ASSERTION(!mImage, "imgRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "imgRequest::Init -- No channel");

  mChannel = aChannel;

  // XXX do not init the image here.  this has to be done from the image decoder.
  mImage = do_CreateInstance("@mozilla.org/gfx/image;2");

  return NS_OK;
}

nsresult imgRequest::AddObserver(imgIDecoderObserver *observer)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddObserver", "observer", observer);

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

  if (mObservers.Count() == 1) {
    PRUint32 nframes;
    mImage->GetNumFrames(&nframes);
    if (nframes > 1) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::AddObserver -- starting animation\n", this));

      mImage->StartAnimation();
    }
  }

  if (mState & onStopRequest) {
    nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(observer));
    PR_ASSERT(observer);
    ob->OnStopRequest(nsnull, nsnull, NS_OK, nsnull);
  } 

  return NS_OK;
}

nsresult imgRequest::RemoveObserver(imgIDecoderObserver *observer, nsresult status)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveObserver", "observer", observer);

  mObservers.RemoveElement(NS_STATIC_CAST(void*, observer));

  if (mObservers.Count() == 0) {
    if (mImage) {
      PRUint32 nframes;
      mImage->GetNumFrames(&nframes);
      if (nframes > 1) {
        PR_LOG(gImgLog, PR_LOG_DEBUG,
               ("[this=%p] imgRequest::RemoveObserver -- stopping animation\n", this));

        mImage->StopAnimation();
      }
    }

    if (mChannel && mProcessing) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- load in progress.  canceling\n", this));

      this->RemoveFromCache();
      this->Cancel(NS_BINDING_ABORTED);
    }
  }

  return NS_OK;
}

PRBool imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

  if (mChannel) {
    mChannel->GetOriginalURI(getter_AddRefs(mURI));
  }

#if defined(PR_LOGGING)
  nsXPIDLCString spec;
  mURI->GetSpec(getter_Copies(spec));

  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::RemoveFromCache -- removing %s from cache\n", this, spec.get()));
#endif

  return ImageCache::Remove(mURI);
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequest::GetName(PRUnichar * *aName)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isPending (); */
NS_IMETHODIMP imgRequest::IsPending(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP imgRequest::GetStatus(nsresult *aStatus)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequest::Cancel(nsresult status)
{
  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  if (mImage) {
    PRUint32 nframes;
    mImage->GetNumFrames(&nframes);
    if (nframes > 1) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- stopping animation\n", this));

      mImage->StopAnimation();
    }
  }

  if (mChannel && mProcessing)
    mChannel->Cancel(NS_BINDING_ABORTED); // should prolly use status here

  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP imgRequest::Suspend()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP imgRequest::Resume()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/** imgIRequest methods **/

/* readonly attribute gfxIImageContainer image; */
NS_IMETHODIMP imgRequest::GetImage(gfxIImageContainer * *aImage)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImage\n", this));

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequest::GetImageStatus(PRUint32 *aStatus)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImageStatus\n", this));

  *aStatus = mStatus;
  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequest::GetURI(nsIURI **aURI)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetURI\n", this));

  if (mChannel)
    mChannel->GetOriginalURI(aURI);
  else if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


/** gfxIImageContainerObserver methods **/

/* [noscript] void frameChanged (in gfxIImageContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequest::FrameChanged(gfxIImageContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->FrameChanged(container, cx, newframe, dirtyRect);
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");

  mState |= onStartDecode;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartDecode(request, cx);
  }

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in gfxIImageContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, nsISupports *cx, gfxIImageContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  mState |= onStartContainer;

  mStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStartFrame(request, cx, frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnDataAvailable(request, cx, frame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopFrame(request, cx, frame);
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in gfxIImageContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request, nsISupports *cx, gfxIImageContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");

  mState |= onStopContainer;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *request, nsISupports *cx, nsresult status, const PRUnichar *statusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");

  mState |= onStopDecode;

  if (NS_FAILED(status))
    mStatus |= imgIRequest::STATUS_ERROR;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopDecode(request, cx, status, statusArg);
  }

  return NS_OK;
}






/** nsIStreamObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  NS_ASSERTION(!mDecoder, "imgRequest::OnStartRequest -- we already have a decoder");

  /* notify our kids */
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *iob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (iob) {
      nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(iob));
      if (ob) ob->OnStartRequest(aRequest, ctxt);
    }
  }

  /* do our real work */
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

  if (mChannel && (mChannel != chan)) {
    NS_WARNING("imgRequest::OnStartRequest -- (mChannel != NULL) && (mChannel != chan)");
  }

  if (!mChannel) {
    PR_LOG(gImgLog, PR_LOG_ALWAYS,
           (" `->  Channel already stopped or no channel!?.\n"));

    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(chan));
  if (httpChannel) {
    PRUint32 httpStatus;
    httpChannel->GetResponseStatus(&httpStatus);
    if (httpStatus == 404) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::OnStartRequest -- http status = 404. canceling.\n", this));

      mStatus |= imgIRequest::STATUS_ERROR;
      // this->Cancel(NS_BINDING_ABORTED);
      this->RemoveFromCache();

      return NS_BINDING_ABORTED;
    }

    /* get the http expires date */
    nsCOMPtr<nsIAtom> expiresAtom(dont_AddRef(NS_NewAtom(NS_LITERAL_STRING("Expires"))));
    nsXPIDLCString expires;
    httpChannel->GetResponseHeader(expiresAtom, getter_Copies(expires));
    if (expires.get()) {
      printf("%s\n", expires.get());
    }
  }

  nsXPIDLCString contentType;
  nsresult rv = mChannel->GetContentType(getter_Copies(contentType));

  if (NS_FAILED(rv)) {
    PR_LOG(gImgLog, PR_LOG_ERROR,
           ("[this=%p] imgRequest::OnStartRequest -- Content type unavailable from the channel\n",
            this));

    this->RemoveFromCache();

    return NS_BINDING_ABORTED; //NS_BASE_STREAM_CLOSED;
  }
#if defined(PR_LOGGING)
  else {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequest::OnStartRequest -- Content type is %s\n", this, contentType.get()));
  }
#endif

  nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
  conid += contentType.get();

  mDecoder = do_CreateInstance(conid);

  if (!mDecoder) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnStartRequest -- Decoder not available\n", this));

    // no image decoder for this mimetype :(
    // this->Cancel(NS_BINDING_ABORTED);
    this->RemoveFromCache();

    // XXX notify the person that owns us now that wants the gfxIImageContainer off of us?

    //return NS_ERROR_IMAGELIB_NO_DECODER;
    return NS_ERROR_FAILURE;
  }

  mDecoder->Init(NS_STATIC_CAST(imgIRequest*, this));

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopRequest\n", this));

  NS_ASSERTION(mChannel && mProcessing, "imgRequest::OnStopRequest -- received multiple OnStopRequest");

  mState |= onStopRequest;

  mProcessing = PR_FALSE;

  switch(status) {
  case NS_BINDING_ABORTED:
  case NS_BINDING_FAILED:
  case NS_ERROR_IMAGELIB_NO_DECODER:

    mStatus |= imgIRequest::STATUS_ERROR;

    this->RemoveFromCache();
    this->Cancel(status); // stops animations

    break;
  case NS_BINDING_SUCCEEDED:
    mStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
    break;
  default:
    printf("weird status return %i\n", status);
    break;
  }

  mChannel->GetOriginalURI(getter_AddRefs(mURI));
  mChannel = nsnull; // we no longer need the channel

  if (mDecoder) {
    mDecoder->Flush();
    mDecoder->Close();
    mDecoder = nsnull; // release the decoder so that it can rest peacefully ;)
  }

  /* notify the kids */
  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *iob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (iob) {
      nsCOMPtr<nsIStreamObserver> ob(do_QueryInterface(iob));
      if (ob) ob->OnStopRequest(aRequest, ctxt, status, statusArg);
    }
  }

  return NS_OK;
}




/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgRequest::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnDataAvailable\n", this));

  NS_ASSERTION(mChannel, "imgRequest::OnDataAvailable -- no channel!");

  if (!mDecoder) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- no decoder\n", this));

    return NS_BASE_STREAM_CLOSED;
  }

  PRUint32 wrote;
  nsresult rv = mDecoder->WriteFrom(inStr, count, &wrote);

  return NS_OK;
}
