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

#include "imgRequestProxy.h"

#include "nsIChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"

#include "imgILoader.h"
#include "nsIComponentManager.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

#include "nsString.h"
#include "nsXPIDLString.h"

#include "gfxIImageFrame.h"

#ifdef MOZ_NEW_CACHE
#include "nsICachingChannel.h"
#endif
#include "imgCache.h"

#include "ImageLogging.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS7(imgRequest, imgIRequest, nsIRequest,
                   imgIDecoderObserver, imgIContainerObserver,
                   nsIStreamListener, nsIRequestObserver,
                   nsISupportsWeakReference)

imgRequest::imgRequest() : 
  mObservers(0), mLoading(PR_FALSE), mProcessing(PR_FALSE), mStatus(imgIRequest::STATUS_NONE), mState(0)
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
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::Init\n", this));

  NS_ASSERTION(!mImage, "imgRequest::Init -- Multiple calls to init");
  NS_ASSERTION(aChannel, "imgRequest::Init -- No channel");

  mChannel = aChannel;

  /* set our loading flag to true here.
     Setting it here lets checks to see if the load is in progress
     before OnStartRequest gets called, letting 'this' properly get removed
     from the cache in certain cases.
  */
  mLoading = PR_TRUE;                      

#ifdef MOZ_NEW_CACHE
  mCacheEntry = aCacheEntry;
#endif

  return NS_OK;
}

nsresult imgRequest::AddProxy(imgRequestProxy *proxy)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  mObservers.AppendElement(NS_STATIC_CAST(void*, proxy));

  // OnStartDecode
  if (mState & onStartDecode)
    proxy->OnStartDecode(nsnull, nsnull);

  // OnStartContainer
  if (mState & onStartContainer)
    proxy->OnStartContainer(nsnull, nsnull, mImage);

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
    proxy->OnStartFrame(nsnull, nsnull, frame);

    if (!(mState & onStopContainer)) {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // XXX we should only send the currently decoded rectangle here.
      proxy->OnDataAvailable(nsnull, nsnull, frame, &r);
    } else {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // We're done loading this image, send the the whole rect
      proxy->OnDataAvailable(nsnull, nsnull, frame, &r);

      // OnStopFrame
      proxy->OnStopFrame(nsnull, nsnull, frame);
    }
  }

  // OnStopContainer
  if (mState & onStopContainer)
    proxy->OnStopContainer(nsnull, nsnull, mImage);

  // OnStopDecode
  if (mState & onStopDecode)
    proxy->OnStopDecode(nsnull, nsnull, GetResultFromStatus(), nsnull);

  if (mImage && (mObservers.Count() == 1)) {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequest::AddObserver -- starting animation\n", this));

    mImage->StartAnimation();
  }

  if (mState & onStopRequest) {
    proxy->OnStopRequest(nsnull, nsnull, GetResultFromStatus());
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
    proxy->OnStopDecode(nsnull, nsnull, NS_IMAGELIB_ERROR_FAILURE, nsnull);
  }

  // make sure that observer gets an OnStopRequest message sent to it
  if (!(mState & onStopRequest)) {
    proxy->OnStopRequest(nsnull, nsnull, NS_BINDING_ABORTED);
  }

  if (mObservers.Count() == 0) {
    if (mImage) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- stopping animation\n", this));

      mImage->StopAnimation();
    }

    /* If |aStatus| is a failure code, then cancel the load if it is still in progress.
       Otherwise, let the load continue, keeping 'this' in the cache with no observers.
       This way, if a proxy is destroyed without calling cancel on it, it won't leak
       and won't leave a bad pointer in mObservers.
     */
    if (mChannel && mLoading && NS_FAILED(aStatus)) {
      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::RemoveObserver -- load in progress.  canceling\n", this));

      this->Cancel(NS_BINDING_ABORTED);
    }

#ifdef MOZ_NEW_CACHE
    /* break the cycle from the cache entry. */
    mCacheEntry = nsnull;
#endif
  }

  return NS_OK;
}

void imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

#ifdef MOZ_NEW_CACHE

  if (mCacheEntry) {
    mCacheEntry->Doom();
    mCacheEntry = nsnull;
  }

#endif
}

nsresult imgRequest::GetResultFromStatus()
{
  nsresult rv = NS_OK;

  if (mStatus & imgIRequest::STATUS_ERROR)
    rv = NS_IMAGELIB_ERROR_FAILURE;
  else if (mStatus & imgIRequest::STATUS_LOAD_COMPLETE)
    rv = NS_IMAGELIB_SUCCESS_LOAD_FINISHED;

  return rv;
}


/**  nsIRequest / imgIRequest methods **/

/* readonly attribute wstring name; */
NS_IMETHODIMP imgRequest::GetName(PRUnichar * *aName)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetName");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* boolean isPending (); */
NS_IMETHODIMP imgRequest::IsPending(PRBool *_retval)
{
    NS_NOTYETIMPLEMENTED("imgRequest::IsPending");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute nsresult status; */
NS_IMETHODIMP imgRequest::GetStatus(nsresult *aStatus)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetStatus");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cancel (in nsresult status); */
NS_IMETHODIMP imgRequest::Cancel(nsresult status)
{
  /* The Cancel() method here should only be called by this class. */

  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  if (mImage) {
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequest::Cancel -- stopping animation\n", this));

    mImage->StopAnimation();
  }

  mStatus |= imgIRequest::STATUS_ERROR;

  RemoveFromCache();

  if (mChannel && mLoading)
    mChannel->Cancel(status);

  return NS_OK;
}

/* void suspend (); */
NS_IMETHODIMP imgRequest::Suspend()
{
    NS_NOTYETIMPLEMENTED("imgRequest::Suspend");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void resume (); */
NS_IMETHODIMP imgRequest::Resume()
{
    NS_NOTYETIMPLEMENTED("imgRequest::Resume");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsILoadGroup loadGroup */
NS_IMETHODIMP imgRequest::GetLoadGroup(nsILoadGroup **loadGroup)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetLoadGroup");
    return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP imgRequest::SetLoadGroup(nsILoadGroup *loadGroup)
{
    NS_NOTYETIMPLEMENTED("imgRequest::SetLoadGroup");
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* attribute nsLoadFlags loadFlags */
NS_IMETHODIMP imgRequest::GetLoadFlags(nsLoadFlags *flags)
{
    *flags = LOAD_NORMAL;
    return NS_OK;
}
NS_IMETHODIMP imgRequest::SetLoadFlags(nsLoadFlags flags)
{
    return NS_OK;
}

/** imgIRequest methods **/

/* attribute imgIContainer image; */
NS_IMETHODIMP imgRequest::GetImage(imgIContainer * *aImage)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::GetImage\n", this));

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

NS_IMETHODIMP imgRequest::SetImage(imgIContainer *aImage)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::SetImage\n", this));

  mImage = aImage;
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
    return mChannel->GetOriginalURI(aURI);

  if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

/* readonly attribute imgIDecoderObserver decoderObserver; */
NS_IMETHODIMP imgRequest::GetDecoderObserver(imgIDecoderObserver **aDecoderObserver)
{
    NS_NOTYETIMPLEMENTED("imgRequest::GetDecoderObserver");
    return NS_ERROR_NOT_IMPLEMENTED;
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
    if (proxy) proxy->OnStartDecode(request, cx);
  }

#ifdef MOZ_NEW_CACHE
  /* In the case of streaming jpegs, it is possible to get multiple OnStartDecodes which
     indicates the beginning of a new decode.
     The cache entry's size therefore needs to be reset to 0 here.  If we do not do this,
     the code in imgRequest::OnStopFrame will continue to increase the data size cumulatively.
   */
  if (mCacheEntry)
    mCacheEntry->SetDataSize(0);
#endif

  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in nsISupports cx, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, nsISupports *cx, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  mState |= onStartContainer;

  mStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartContainer(request, cx, image);
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
    if (proxy) proxy->OnStartFrame(request, cx, frame);
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
    if (proxy) proxy->OnDataAvailable(request, cx, frame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in nsISupports cx, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request, nsISupports *cx, gfxIImageFrame *frame)
{
  NS_ASSERTION(frame, "imgRequest::OnStopFrame called with NULL frame");

  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

#ifdef MOZ_NEW_CACHE
  if (mCacheEntry) {
    PRUint32 cacheSize = 0;

    mCacheEntry->GetDataSize(&cacheSize);

    PRUint32 imageSize = 0;
    PRUint32 alphaSize = 0;

    frame->GetImageDataLength(&imageSize);
    frame->GetAlphaDataLength(&alphaSize);

    mCacheEntry->SetDataSize(cacheSize + imageSize + alphaSize);
  }
#endif

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopFrame(request, cx, frame);
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
    if (proxy) proxy->OnStopContainer(request, cx, image);
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsISupports cx, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *aRequest, nsISupports *aCX, nsresult aStatus, const PRUnichar *aStatusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");

  NS_ASSERTION(!(mState & onStopDecode), "OnStopDecode called multiple times.");

  mState |= onStopDecode;

  if (NS_FAILED(aStatus))
    mStatus |= imgIRequest::STATUS_ERROR;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopDecode(aRequest, aCX, GetResultFromStatus(), aStatusArg);
  }

  return NS_OK;
}






/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  NS_ASSERTION(!mDecoder, "imgRequest::OnStartRequest -- we already have a decoder");

  /* set our state variables to their initial values. */
  mStatus = imgIRequest::STATUS_NONE;
  mState = 0;

  /* set our loading flag to true */
  mLoading = PR_TRUE;

  /* notify our kids */
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartRequest(aRequest, ctxt);
  }

#if defined(MOZ_NEW_CACHE)
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
#endif

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  PR_LOG(gImgLog, PR_LOG_DEBUG,
         ("[this=%p] imgRequest::OnStopRequest\n", this));

  mState |= onStopRequest;

  /* set our loading flag to false */
  mLoading = PR_FALSE;

  /* set our processing flag to false */
  mProcessing = PR_FALSE;

  if (mChannel) {
    mChannel->GetOriginalURI(getter_AddRefs(mURI));
    mChannel = nsnull; // we no longer need the channel
  }

  if (NS_FAILED(status)) {
    this->Cancel(status); // sets status, stops animations, removes from cache
  } else {
    mStatus |= imgIRequest::STATUS_LOAD_COMPLETE;
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
  for (PRInt32 i = 0; i < count; i++) {
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
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

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

        this->Cancel(NS_BINDING_ABORTED);

        return NS_BINDING_ABORTED;
      }

      PR_LOG(gImgLog, PR_LOG_DEBUG,
             ("[this=%p] imgRequest::OnDataAvailable -- Got content type from the channel\n",
              this));

      mContentType = contentType;
    }

#if defined(PR_LOGGING)
    PR_LOG(gImgLog, PR_LOG_DEBUG,
           ("[this=%p] imgRequest::OnDataAvailable -- Content type is %s\n", this, mContentType.get()));
#endif

    nsCAutoString conid("@mozilla.org/image/decoder;2?type=");
    conid += mContentType.get();

    mDecoder = do_CreateInstance(conid);

    if (!mDecoder) {
      PR_LOG(gImgLog, PR_LOG_WARNING,
             ("[this=%p] imgRequest::OnDataAvailable -- Decoder not available\n", this));

      // no image decoder for this mimetype :(
      this->Cancel(NS_BINDING_ABORTED);

      return NS_IMAGELIB_ERROR_NO_DECODER;
    }

    mDecoder->Init(NS_STATIC_CAST(imgIRequest*, this));
  }

  if (!mDecoder) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- no decoder\n", this));

    this->Cancel(NS_BINDING_ABORTED);

    return NS_BINDING_ABORTED;
  }

  PRUint32 wrote;
  nsresult rv = mDecoder->WriteFrom(inStr, count, &wrote);
#ifdef DEBUG_pavlov
  NS_ASSERTION(NS_SUCCEEDED(rv), "mDecoder->WriteFrom failed");
#endif

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
