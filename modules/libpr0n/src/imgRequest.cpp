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

#include "nsIChannel.h"
#include "nsIInputStream.h"
#include "imgILoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsString.h"

#include "ImageCache.h"

#include "prlog.h"

#if defined(PR_LOGGING)
#define PR_LOG_ANNOYING 10
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS5(imgRequest, imgIRequest, 
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

#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::Init\n", this));
#endif
  NS_ASSERTION(!mImage, "imgRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "imgRequest::Init -- No channel");

  mChannel = aChannel;

  // XXX do not init the image here.  this has to be done from the image decoder.
  mImage = do_CreateInstance("@mozilla.org/gfx/image;2");

  return NS_OK;
}

nsresult imgRequest::AddObserver(imgIDecoderObserver *observer)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::AddObserver    (observer=%p)\n", this, observer));
#endif

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

nsresult imgRequest::RemoveObserver(imgIDecoderObserver *observer, nsresult status)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::RemoveObserver (observer=%p)\n", this, observer));
#endif

  mObservers.RemoveElement(NS_STATIC_CAST(void*, observer));

  if ((mObservers.Count() == 0) && mChannel && mProcessing) {
    this->Cancel(NS_BINDING_ABORTED);
  }
  return NS_OK;
}



/** imgIRequest methods **/


/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequest::Cancel(nsresult status)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::Cancel\n", this));
#endif

  if (mChannel) {
    mChannel->GetOriginalURI(getter_AddRefs(mURI));
  }

#if defined(PR_LOGGING)
  nsXPIDLCString spec;
  mURI->GetSpec(getter_Copies(spec));

  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("`->  Removing %s from cache\n", spec.get()));
#endif



  ImageCache::Remove(mURI);

  nsresult rv = NS_OK;
  if (mChannel && mProcessing)
    rv = mChannel->Cancel(status);

  return rv;
}

/* readonly attribute gfxIImageContainer image; */
NS_IMETHODIMP imgRequest::GetImage(gfxIImageContainer * *aImage)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImage\n", this));
#endif

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

/* readonly attribute unsigned long imageStatus; */
NS_IMETHODIMP imgRequest::GetImageStatus(PRUint32 *aStatus)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImageStatus\n", this));
#endif

  *aStatus = mStatus;
  return NS_OK;
}

/* readonly attribute nsIURI URI; */
NS_IMETHODIMP imgRequest::GetURI(nsIURI **aURI)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetURI\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::FrameChanged\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStartDecode\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStartContainer\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStartFrame\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnDataAvailable\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopFrame\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopContainer\n", this));
#endif

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
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopDecode\n", this));
#endif

  mState |= onStopDecode;

  if (NS_FAILED(status))
    mStatus = imgIRequest::STATUS_ERROR;

  PRInt32 i = -1;
  PRInt32 count = mObservers.Count();

  while (++i < count) {
    imgIDecoderObserver *ob = NS_STATIC_CAST(imgIDecoderObserver*, mObservers[i]);
    if (ob) ob->OnStopDecode(request, cx, status, statusArg);
  }

  return NS_OK;
}






/** nsIStreamObserver methods **/

/* void onStartRequest (in nsIChannel channel, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIChannel *channel, nsISupports *ctxt)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStartRequest\n", this));
#endif

  NS_ASSERTION(!mDecoder, "imgRequest::OnStartRequest -- we already have a decoder");

  if (mChannel && (mChannel != channel)) {
    NS_WARNING("imgRequest::OnStartRequest -- (mChannel != NULL) && (mChannel != channel)");
  }

  if (!mChannel) {
#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_ALWAYS,
           (" `->  Channel already canceled.\n"));
#endif

    return NS_ERROR_FAILURE;
  }

  nsXPIDLCString contentType;
  nsresult rv = mChannel->GetContentType(getter_Copies(contentType));

  if (NS_FAILED(rv)) {
#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_ALWAYS,
           (" `->  Error getting content type\n"));
#endif
    this->Cancel(NS_BINDING_ABORTED);

    return NS_ERROR_FAILURE;
  } else {
#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           (" `->  Content type is %s\n", contentType.get()));
#endif
  }

  nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
  conid += contentType.get();

  mDecoder = do_CreateInstance(conid);

  if (!mDecoder) {
    // no image decoder for this mimetype :(
    this->Cancel(NS_BINDING_ABORTED);

    // XXX notify the person that owns us now that wants the gfxIImageContainer off of us?

    return NS_ERROR_FAILURE;
  }

  mDecoder->Init(NS_STATIC_CAST(imgIRequest*, this));
  return NS_OK;
}

/* void onStopRequest (in nsIChannel channel, in nsISupports ctxt, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *statusArg)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopRequest\n", this));
#endif

  NS_ASSERTION(mChannel && mProcessing, "imgRequest::OnStopRequest -- received multiple OnStopRequest");

  mProcessing = PR_FALSE;

  mChannel->GetOriginalURI(getter_AddRefs(mURI));
  mChannel = nsnull; // we no longer need the channel

  if (!mDecoder) return NS_ERROR_FAILURE;

  mDecoder->Close();

  mDecoder = nsnull; // release the decoder so that it can rest peacefully ;)

  return NS_OK;
}




/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIChannel channel, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgRequest::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
#if defined(PR_LOGGING)
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnDataAvailable\n", this));
#endif

  if (!mChannel) {
#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_WARNING,
           (" `->  no channel\n"));
#endif
  }

  if (!mDecoder) {
#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_WARNING,
           (" `->  no decoder\n"));
#endif
    return NS_ERROR_FAILURE;
  }

  PRUint32 wrote;
  return mDecoder->WriteFrom(inStr, count, &wrote);
}
