/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "imgRequest.h"

#include "imgLoader.h"
#include "imgCache.h"
#include "imgRequestProxy.h"

#include "imgILoader.h"
#include "ImageErrors.h"
#include "ImageLogging.h"

#include "gfxIImageFrame.h"

#include "netCore.h"

#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIHttpChannel.h"

#include "nsIComponentManager.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"

#include "nsAutoLock.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h" // PL_strcasestr(...)

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_THREADSAFE_ISUPPORTS6(imgRequest, imgILoad,
                              imgIDecoderObserver, imgIContainerObserver,
                              nsIStreamListener, nsIRequestObserver,
                              nsISupportsWeakReference)

imgRequest::imgRequest() : 
  mObservers(0),
  mLoading(PR_FALSE), mProcessing(PR_FALSE),
  mImageStatus(imgIRequest::STATUS_NONE), mState(0),
  mCacheId(0), mValidator(nsnull), mIsMultiPartChannel(PR_FALSE)
{
  /* member initializers and constructor code */
}

imgRequest::~imgRequest()
{
  /* destructor code */
}

nsresult imgRequest::Init(nsIChannel *aChannel,
                          nsICacheEntryDescriptor *aCacheEntry,
                          void *aCacheId,
                          void *aLoadId)
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

  mCacheId = aCacheId;

  SetLoadId(aLoadId);

  return NS_OK;
}

nsresult imgRequest::AddProxy(imgRequestProxy *proxy, PRBool aNotify)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  mObservers.AppendElement(NS_STATIC_CAST(void*, proxy));

  if (aNotify)
    NotifyProxyListener(proxy);

  return NS_OK;
}

nsresult imgRequest::RemoveProxy(imgRequestProxy *proxy, nsresult aStatus, PRBool aNotify)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy", "proxy", proxy);

  mObservers.RemoveElement(NS_STATIC_CAST(void*, proxy));

  /* Check mState below before we potentially call Cancel() below. Since
     Cancel() may result in OnStopRequest being called back before Cancel()
     returns, leaving mState in a different state then the one it was in at
     this point.
   */

  if (aNotify) {
    // make sure that observer gets an OnStopDecode message sent to it
    if (!(mState & onStopDecode)) {
      proxy->OnStopDecode(aStatus, nsnull);
    }

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

  // If a proxy is removed for a reason other than its owner being
  // changed, remove the proxy from the loadgroup.
  if (aStatus != NS_IMAGELIB_CHANGING_OWNER)
    proxy->RemoveFromLoadGroup();

  return NS_OK;
}

nsresult imgRequest::NotifyProxyListener(imgRequestProxy *proxy)
{
  nsCOMPtr<imgIRequest> kungFuDeathGrip(proxy);

  // OnStartDecode
  if (mState & onStartDecode)
    proxy->OnStartDecode();

  // OnStartContainer
  if (mState & onStartContainer)
    proxy->OnStartContainer(mImage);

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
    proxy->OnStartFrame(frame);

    if (!(mState & onStopContainer)) {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // XXX we should only send the currently decoded rectangle here.
      proxy->OnDataAvailable(frame, &r);
    } else {
      // OnDataAvailable
      nsRect r;
      frame->GetRect(r);  // We're done loading this image, send the the whole rect
      proxy->OnDataAvailable(frame, &r);

      // OnStopFrame
      proxy->OnStopFrame(frame);
    }
  }

  // OnStopContainer
  if (mState & onStopContainer)
    proxy->OnStopContainer(mImage);

  // OnStopDecode
  if (mState & onStopDecode)
    proxy->OnStopDecode(GetResultFromImageStatus(mImageStatus), nsnull);

  if (mImage && (mObservers.Count() == 1)) {
    LOG_MSG(gImgLog, "imgRequest::AddProxy", "resetting animation");

    mImage->ResetAnimation();
  }

  if (mState & onStopRequest) {
    proxy->OnStopRequest(nsnull, nsnull, GetResultFromImageStatus(mImageStatus));
  }

  return NS_OK;
}

nsresult imgRequest::GetResultFromImageStatus(PRUint32 aStatus) const
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

NS_IMETHODIMP imgRequest::GetIsMultiPartChannel(PRBool *aIsMultiPartChannel)
{
  LOG_FUNC(gImgLog, "imgRequest::GetIsMultiPartChannel");

  *aIsMultiPartChannel = mIsMultiPartChannel;

  return NS_OK;
}

/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in gfxIImageFrame newframe, in nsRect dirtyRect); */
NS_IMETHODIMP imgRequest::FrameChanged(imgIContainer *container,
                                       gfxIImageFrame *newframe,
                                       nsRect * dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->FrameChanged(container, newframe, dirtyRect);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(),
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");

  mState |= onStartDecode;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartDecode();

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

/* void onStartContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  NS_ASSERTION(image, "imgRequest::OnStartContainer called with a null image!");
  if (!image) return NS_ERROR_UNEXPECTED;

  mState |= onStartContainer;

  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartContainer(image);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");

  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request,
                                       gfxIImageFrame *frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStartFrame(frame);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in gfxIImageFrame frame, [const] in nsRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request,
                                          gfxIImageFrame *frame,
                                          const nsRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnDataAvailable(frame, rect);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in gfxIImageFrame frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request,
                                      gfxIImageFrame *frame)
{
  NS_ASSERTION(frame, "imgRequest::OnStopFrame called with NULL frame");
  if (!frame) return NS_ERROR_UNEXPECTED;

  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE;

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
    if (proxy) proxy->OnStopFrame(frame);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request,
                                          imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");

  mState |= onStopContainer;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; i++) {
    imgRequestProxy *proxy = NS_STATIC_CAST(imgRequestProxy*, mObservers[i]);
    if (proxy) proxy->OnStopContainer(image);

    // If this assertion fires, it means that imgRequest notifications could
    // be dropped!
    NS_ASSERTION(count == mObservers.Count(), 
                 "The observer list changed while being iterated over!");
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *aRequest,
                                       nsresult aStatus,
                                       const PRUnichar *aStatusArg)
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
    if (proxy) proxy->OnStopDecode(GetResultFromImageStatus(mImageStatus), aStatusArg);

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
  nsresult rv;

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
    nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
    if (mpchan)
      mpchan->GetBaseChannel(getter_AddRefs(mChannel));
    else
      mChannel = do_QueryInterface(aRequest);
  }

  nsCAutoString contentType;
  mChannel->GetContentType(contentType);
  if (contentType.Equals(NS_LITERAL_CSTRING("multipart/x-mixed-replace"),
                         nsCaseInsensitiveCStringComparator()))
      mIsMultiPartChannel = PR_TRUE;

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
  if (mCacheEntry) {
    nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aRequest));
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
    //
    // Determine whether the cache entry must be revalidated when it expires.
    // If so, then the cache entry must *not* be used during HISTORY loads if
    // it has expired.
    //
    // Currently, only HTTP specifies this information...
    //
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
    if (httpChannel) {
      PRBool bMustRevalidate = PR_FALSE;

      rv = httpChannel->IsNoStoreResponse(&bMustRevalidate);

      if (!bMustRevalidate) {
        rv = httpChannel->IsNoCacheResponse(&bMustRevalidate);
      }

      if (!bMustRevalidate) {
        nsCAutoString cacheHeader;

        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Cache-Control"),
                                            cacheHeader);
        if (PL_strcasestr(cacheHeader.get(), "must-revalidate")) {
          bMustRevalidate = PR_TRUE;
        }
      }

      if (bMustRevalidate) {
        mCacheEntry->SetMetaDataElement("MustValidateIfExpired", "true");
      }
    }
  }


  // Shouldn't we be dead already if this gets hit?  Probably multipart/x-mixed-replace...
  if (mObservers.Count() == 0) {
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
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
    this->OnStopDecode(nsnull, status, nsnull);
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

      nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));

      nsresult rv = NS_ERROR_FAILURE;
      if (chan) {
        rv = chan->GetContentType(mContentType);
      }

      if (NS_FAILED(rv)) {
        PR_LOG(gImgLog, PR_LOG_ERROR,
               ("[this=%p] imgRequest::OnDataAvailable -- Content type unavailable from the channel\n",
                this));

        this->Cancel(NS_IMAGELIB_ERROR_FAILURE);

        return NS_BINDING_ABORTED;
      }

      LOG_MSG(gImgLog, "imgRequest::OnDataAvailable", "Got content type from the channel");
    }

    LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "content type", mContentType.get());

    nsCAutoString conid(NS_LITERAL_CSTRING("@mozilla.org/image/decoder;2?type=") + mContentType);

    mDecoder = do_CreateInstance(conid.get());

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
  imgLoader::GetMimeTypeFromContent(buf, len, mContentType);
}
