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

#include "imgCache.h"
#include "imgRequestProxy.h"

#include "imgILoader.h"
#include "ImageErrors.h"
#include "ImageLogging.h"

#include "gfxIImageFrame.h"

#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"

#include "nsIComponentManager.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"

#include "nsAutoLock.h"
#include "nsString.h"
#include "nsXPIDLString.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_THREADSAFE_ISUPPORTS5(imgRequest, imgIDecoderObserver, imgIContainerObserver,
                              nsIStreamListener, nsIRequestObserver,
                              nsISupportsWeakReference)

imgRequest::imgRequest() : 
  mObservers(0),
  mLoading(PR_FALSE), mProcessing(PR_FALSE),
  mImageStatus(imgIRequest::STATUS_NONE), mState(0)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

imgRequest::~imgRequest()
{
  /* destructor code */
}

nsresult imgRequest::Init(nsIChannel *aChannel, nsICacheEntryDescriptor *aCacheEntry)
{
  LOG_FUNC(gImgLog, "imgRequest::Init");

  NS_ASSERTION(!mImage, "imgRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "imgRequest::Init -- No channel");

  mChannel = aChannel;

  /* set our loading flag to true here.
     Setting it here lets checks to see if the load is in progress
     before OnStartRequest gets called, letting 'this' properly get removed
     from the cache in certain cases.
  */
  mLoading = PR_TRUE;                      

  mCacheEntry = aCacheEntry;

  return NS_OK;
}

nsresult imgRequest::AddProxy(imgRequestProxy *proxy)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  mObservers.AppendElement(NS_STATIC_CAST(void*, proxy));

  // OnStartDecode
  if (mState & onStartDecode)
    proxy->OnStartDecode(nsnull);

  // OnStartContainer
  if (mState & onStartContainer)
    proxy->OnStartContainer(nsnull, mImage);

  // Send frame messages (OnStartFrame, OnDataAvailable, OnStopFrame)
  PRUint32 nframes = 0;
  if (mImage)
    mImage->GetNumFrames(&nframes);

  if (nframes > 0) {
    nsCOMPtr<gfxIImageFrame> frame;

    // get the current frame or only frame
    mImage->GetCurrentFrame(getter_AddRefs(frame));
    NS_ASSERTION(frame, "GetCurrentFrame gave back a null frame!");

    // OnStartFrame
    proxy->OnStartFrame(nsnull, frame);

    if (!(mState & onStopContainer)) {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // XXX we should only send the currently decoded rectangle here.
      proxy->OnDataAvailable(nsnull, frame, &r);
    } else {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // We're done loading this image, send the the whole rect
      proxy->OnDataAvailable(nsnull, frame, &r);

      // OnStopFrame
      proxy->OnStopFrame(nsnull, frame);
    }
  }

  // OnStopContainer
  if (mState & onStopContainer)
    proxy->OnStopContainer(nsnull, mImage);

  // OnStopDecode
  if (mState & onStopDecode)
    proxy->OnStopDecode(nsnull, GetResultFromImageStatus(mImageStatus), nsnull);

  if (mImage && (mObservers.Count() == 1)) {
    LOG_MSG(gImgLog, "imgRequest::AddProxy", "starting animation");

    mImage->StartAnimation();
  }

  if (mState & onStopRequest) {
    proxy->OnStopRequest(nsnull, nsnull, GetResultFromImageStatus(mImageStatus));
  } 

  return NS_OK;
}

nsresult imgRequest::RemoveProxy(imgRequestProxy *proxy, nsresult aStatus)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy", "proxy", proxy);

  mObservers.RemoveElement(NS_STATIC_CAST(void*, proxy));

  /* Check mState below before we potentially call Cancel() below. Since
     Cancel() may result in OnStopRequest being called back before Cancel()
     returns, leaving mState in a different state then the one it was in at
     this point.
   */

  // make sure that observer gets an OnStopDecode message sent to it
  if (!(mState & onStopDecode)) {
    proxy->OnStopDecode(nsnull, aStatus, nsnull);
  }

  // make sure that observer gets an OnStopRequest message sent to it
  if (!(mState & onStopRequest)) {
    proxy->OnStopRequest(nsnull, nsnull, NS_BINDING_ABORTED);
  }

  if (mObservers.Count() == 0) {
    if (mImage) {
      LOG_MSG(gImgLog, "imgRequest::RemoveProxy", "stopping animation");

      mImage->StopAnimation();
    }

    /* If |aStatus| is a failure code, then cancel the load if it is still in progress.
       Otherwise, let the load continue, keeping 'this' in the cache with no observers.
       This way, if a proxy is destroyed without calling cancel on it, it won't leak
       and won't leave a bad pointer in mObservers.
     */
    if (mChannel && mLoading && NS_FAILED(aStatus)) {
      LOG_MSG(gImgLog, "imgRequest::RemoveProxy", "load in progress.  canceling");

      mImageStatus |= imgIRequest::STATUS_LOAD_PARTIAL;

      this->Cancel(NS_BINDING_ABORTED);
    }

    /* break the cycle from the cache entry. */
    mCacheEntry = nsnull;
  }

  return NS_OK;
}

nsresult imgRequest::GetResultFromImageStatus(PRUint32 aStatus)
{
  nsresult rv = NS_OK;

  if (aStatus & imgIRequest::STATUS_ERROR)
    rv = NS_IMAGELIB_ERROR_FAILURE;
  else if (aStatus & imgIRequest::STATUS_LOAD_COMPLETE)
    rv = NS_IMAGELIB_SUCCESS_LOAD_FINISHED;

  return rv;
}

void imgRequest::Cancel(nsresult aStatus)
{
  /* The Cancel() method here should only be called by this class. */

  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  if (mImage) {
    LOG_MSG(gImgLog, "imgRequest::Cancel", "stopping animation");

    mImage->StopAnimation();
  }

  if (!(mImageStatus & imgIRequest::STATUS_LOAD_PARTIAL))
    mImageStatus |= imgIRequest::STATUS_ERROR;

  RemoveFromCache();

  if (mChannel && mLoading)
    mChannel->Cancel(aStatus);
}

nsresult imgRequest::GetURI(nsIURI **aURI)
{
  LOG_FUNC(gImgLog, "imgRequest::GetURI");

  if (mChannel)
    return mChannel->GetOriginalURI(aURI);

  if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}


void imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

  if (mCacheEntry) {
    mCacheEntry->Doom();
    mCacheEntry = nsnull;
  }
}


/** imgILoad methods **/

NS_IMETHODIMP imgRequest::SetImage(imgIContainer *aImage)
{
  LOG_FUNC(gImgLog, "imgRequest::SetImage");

  mImage = aImage;

  return NS_OK;
}

NS_IMETHODIMP imgRequest::GetImage(imgIContainer **aImage)
{
  LOG_FUNC(gImgLog, "imgRequest::GetImage");

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}


/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsISupports cx, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequest::FrameChanged(imgIContainer *container, nsISupports *cx, gfxIImageFrame *newframe, nsRect * dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->FrameChanged(container, cx, newframe, dirtyRect);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(),
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request, in nsISupports cx); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request, nsISupports *cx)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");

  mState |= onStartDecode;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartDecode(cx);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  /* In the case of streaming jpegs, it is possible to get multiple OnStartDecodes which
     indicates the beginning of a new decode.
     The cache entry's size therefore needs to be reset to 0 here.  If we do not do this,
     the code in imgRequest::OnStopFrame will continue to increase the data size cumulatively.
   */
  if (mCacheEntry)
    mCacheEntry->SetDataSize(0);

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  NS_ASSERTION(image, "imgRequest::OnStartContainer called with a null image!");
  if (!image) return NS_ERROR_UNEXPECTED;

  mState |= onStartContainer;

  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartContainer(cx, image);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");

  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartFrame(cx, frame);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame, const nsRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

  nsCOMPtr<imgIDecoderObserver> container = do_QueryInterface(mImage);
  if (container)
    container->OnDataAvailable(request, cx, frame, rect);
  
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnDataAvailable(cx, frame, rect);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  NS_ASSERTION(frame, "imgRequest::OnStopFrame called with NULL frame");
  if (!frame) return NS_ERROR_UNEXPECTED;

  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

  if (mCacheEntry) {
    PRUint32 cacheSize = 0;

    mCacheEntry->GetDataSize(&cacheSize);

    PRUint32 imageSize = 0;
    PRUint32 alphaSize = 0;

    frame->GetImageDataLength(&imageSize);
    frame->GetAlphaDataLength(&alphaSize);

    mCacheEntry->SetDataSize(cacheSize + imageSize + alphaSize);
  }

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopFrame(cx, frame);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");

  mState |= onStopContainer;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopContainer(cx, image);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *aRequest, nsISupports *aCX, nsresult aStatus, const PRUnichar *aStatusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");

  NS_ASSERTION(!(mState & onStopDecode), "OnStopDecode called multiple times.");

  mState |= onStopDecode;

  if (NS_FAILED(aStatus) && !(mImageStatus & imgIRequest::STATUS_LOAD_PARTIAL)) {
    mImageStatus |= imgIRequest::STATUS_ERROR;
  }

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopDecode(aCX, GetResultFromImageStatus(mImageStatus), aStatusArg);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}


/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  NS_ASSERTION(!mDecoder, "imgRequest::OnStartRequest -- we already have a decoder");

  /* if mChannel isn't set here, use aRequest.
     Having mChannel set is important for Canceling the load, and since we set
     mChannel to null in OnStopRequest.  Since with multipart/x-mixed-replace, you
     can get multiple OnStartRequests, we need to reinstance mChannel so that when/if
     Cancel() gets called, we have a channel to cancel and we don't leave the channel
     open forever.
   */
  if (!mChannel) {
    mChannel = do_QueryInterface(aRequest);
  }

  /* set our state variables to their initial values. */
  mImageStatus = imgIRequest::STATUS_NONE;
  mState = 0;

  /* set our loading flag to true */
  mLoading = PR_TRUE;

  /* notify our kids */
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartRequest(aRequest, ctxt);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

  /* get the expires info */
  if (mCacheEntry && chan) {
    nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(chan));
    if (cacheChannel) {
      nsCOMPtr<nsISupports> cacheToken;
      cacheChannel->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken) {
        nsCOMPtr<nsICacheEntryDescriptor> entryDesc(do_QueryInterface(cacheToken));
        if (entryDesc) {
          PRUint32 expiration;
          /* get the expiration time from the caching channel's token */
          entryDesc->GetExpirationTime(&expiration);

          /* set the expiration time on our entry */
          mCacheEntry->SetExpirationTime(expiration);
        }
      }
    }
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  LOG_FUNC(gImgLog, "imgRequest::OnStopRequest");

  mState |= onStopRequest;

  /* set our loading flag to false */
  mLoading = PR_FALSE;

  /* set our processing flag to false */
  mProcessing = PR_FALSE;

  if (mChannel) {
    mChannel->GetOriginalURI(getter_AddRefs(mURI));
    mChannel = nsnull; // we no longer need the channel
  }

  // If mImage is still null, we didn't properly load the image.
  if (NS_FAILED(status) || !mImage) {
    this->Cancel(status); // sets status, stops animations, removes from cache
  } else {
    mImageStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
  }

  if (mDecoder) {
    mDecoder->Flush();
    mDecoder->Close();
    mDecoder = nsnull; // release the decoder so that it can rest peacefully ;)
  }
 
  // if there was an error loading the image, (mState & onStopDecode) won't be true.
  // Send an onStopDecode message
  if (!(mState & onStopDecode)) {
    this->OnStopDecode(nsnull, nsnull, status, nsnull);
  }

  /* notify the kids */
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = count-1; i>=0; i--) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    /* calling OnStopRequest may result in the death of |proxy| so don't use the
       pointer after this call.
     */
    if (proxy) proxy->OnStopRequest(aRequest, ctxt, status);
  }

  return NS_OK;
}




/* prototype for this defined below */
static NS_METHOD sniff_mimetype_callback(nsIInputStream* in, void* closure, const char* fromRawSegment,
                                         PRUint32 toOffset, PRUint32 count, PRUint32 *writeCount);


/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgRequest::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "count", count);

  NS_ASSERTION(aRequest, "imgRequest::OnDataAvailable -- no request!");

  if (!mProcessing) {
    LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |First time through... finding mimetype|");

    /* set our processing flag to true if this is the first OnDataAvailable() */
    mProcessing = PR_TRUE;

    /* look at the first few bytes and see if we can tell what the data is from that
     * since servers tend to lie. :(
     */
    PRUint32 out;
    inStr->ReadSegments(sniff_mimetype_callback, this, count, &out);

#ifdef NS_DEBUG
    /* NS_WARNING if the content type from the channel isn't the same if the sniffing */
#endif

    if (mContentType.IsEmpty()) {
      LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |sniffing of mimetype failed|");

      nsXPIDLCString contentType;
      nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

      nsresult rv = NS_ERROR_FAILURE;
      if (chan) {
        rv = chan->GetContentType(getter_Copies(contentType));
      }

      if (NS_FAILED(rv)) {
        PR_LOG(gImgLog, PR_LOG_ERROR,
               ("[this=%p] imgRequest::OnDataAvailable -- Content type unavailable from the channel\n",
                this));

        this->Cancel(NS_IMAGELIB_ERROR_FAILURE);

        return NS_BINDING_ABORTED;
      }

      LOG_MSG(gImgLog, "imgRequest::OnDataAvailable", "Got content type from the channel");

      mContentType = contentType;
    }

    LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "content type", mContentType.get());

    nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
    conid += mContentType.get();

    mDecoder = do_CreateInstance(conid);

    if (!mDecoder) {
      PR_LOG(gImgLog, PR_LOG_WARNING,
             ("[this=%p] imgRequest::OnDataAvailable -- Decoder not available\n", this));

      // no image decoder for this mimetype :(
      this->Cancel(NS_IMAGELIB_ERROR_NO_DECODER);

      return NS_IMAGELIB_ERROR_NO_DECODER;
    }

    nsresult rv = mDecoder->Init(NS_STATIC_CAST(imgILoad*, this));
    if (NS_FAILED(rv)) {
      PR_LOG(gImgLog, PR_LOG_WARNING,
             ("[this=%p] imgRequest::OnDataAvailable -- mDecoder->Init failed\n", this));

      this->Cancel(NS_IMAGELIB_ERROR_FAILURE);

      return NS_BINDING_ABORTED;
    }
  }

  if (!mDecoder) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- no decoder\n", this));

    this->Cancel(NS_IMAGELIB_ERROR_NO_DECODER);

    return NS_BINDING_ABORTED;
  }

  PRUint32 wrote;
  nsresult rv = mDecoder->WriteFrom(inStr, count, &wrote);

  if (NS_FAILED(rv)) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- mDecoder->WriteFrom failed\n", this));

    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);

    return NS_BINDING_ABORTED;
  }

  return NS_OK;
}

static NS_METHOD sniff_mimetype_callback(nsIInputStream* in,
                                         void* closure,
                                         const char* fromRawSegment,
                                         PRUint32 toOffset,
                                         PRUint32 count,
                                         PRUint32 *writeCount)
{
  imgRequest *request = NS_STATIC_CAST(imgRequest*, closure);

  NS_ASSERTION(request, "request is null!");

  if (count > 0)
    request->SniffMimeType(fromRawSegment, count);

  *writeCount = 0;
  return NS_ERROR_FAILURE;
}

void
imgRequest::SniffMimeType(const char *buf, PRUint32 len)
{
  /* Is it a GIF? */
  if (len >= 4 && !nsCRT::strncmp(buf, "GIF8", 4))  {
    mContentType = NS_LITERAL_CSTRING("image/gif");
    return;
  }

  /* or a PNG? */
  if (len >= 4 && ((unsigned char)buf[0]==0x89 &&
                   (unsigned char)buf[1]==0x50 &&
                   (unsigned char)buf[2]==0x4E &&
                   (unsigned char)buf[3]==0x47))
  { 
    mContentType = NS_LITERAL_CSTRING("image/png");
    return;
  }

  /* maybe a JPEG (JFIF)? */
  /* JFIF files start with SOI APP0 but older files can start with SOI DQT
   * so we test for SOI followed by any marker, i.e. FF D8 FF
   * this will also work for SPIFF JPEG files if they appear in the future.
   *
   * (JFIF is 0XFF 0XD8 0XFF 0XE0 <skip 2> 0X4A 0X46 0X49 0X46 0X00)
   */
  if (len >= 3 &&
     ((unsigned char)buf[0])==0xFF &&
     ((unsigned char)buf[1])==0xD8 &&
     ((unsigned char)buf[2])==0xFF)
  {
    mContentType = NS_LITERAL_CSTRING("image/jpeg");
    return;
  }

  /* or how about ART? */
  /* ART begins with JG (4A 47). Major version offset 2.
   * Minor version offset 3. Offset 4 must be NULL.
   */
  if (len >= 5 &&
   ((unsigned char) buf[0])==0x4a &&
   ((unsigned char) buf[1])==0x47 &&
   ((unsigned char) buf[4])==0x00 )
  {
    mContentType = NS_LITERAL_CSTRING("image/x-jg");
    return;
  }

  /* none of the above?  I give up */
}
