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
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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
#include "imgRequestProxy.h"
#include "imgContainer.h"

#include "imgILoader.h"
#include "ImageErrors.h"
#include "ImageLogging.h"

#include "netCore.h"

#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIHttpChannel.h"

#include "nsIComponentManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIProxyObjectManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"

#include "nsICacheVisitor.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h" // PL_strcasestr(...)

static PRBool gDecodeOnDraw = PR_FALSE;
static PRBool gDiscardable = PR_FALSE;

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS7(imgRequest,
                   imgIDecoderObserver, imgIContainerObserver,
                   nsIStreamListener, nsIRequestObserver,
                   nsISupportsWeakReference,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor)

imgRequest::imgRequest() : 
  mImageStatus(imgIRequest::STATUS_NONE), mState(0), mCacheId(0), 
  mValidator(nsnull), mImageSniffers("image-sniffing-services"),
  mDeferredLocks(0), mDecodeRequested(PR_FALSE),
  mIsMultiPartChannel(PR_FALSE), mLoading(PR_FALSE),
  mHadLastPart(PR_FALSE), mGotData(PR_FALSE), mIsInCache(PR_FALSE)
{
  /* member initializers and constructor code */
}

imgRequest::~imgRequest()
{
  if (mKeyURI) {
    nsCAutoString spec;
    mKeyURI->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::~imgRequest()", "keyuri", spec.get());
  } else
    LOG_FUNC(gImgLog, "imgRequest::~imgRequest()");
}

nsresult imgRequest::Init(nsIURI *aURI,
                          nsIURI *aKeyURI,
                          nsIRequest *aRequest,
                          nsIChannel *aChannel,
                          imgCacheEntry *aCacheEntry,
                          void *aCacheId,
                          void *aLoadId)
{
  LOG_FUNC(gImgLog, "imgRequest::Init");

  NS_ABORT_IF_FALSE(!mImage, "Multiple calls to init");
  NS_ABORT_IF_FALSE(aURI, "No uri");
  NS_ABORT_IF_FALSE(aKeyURI, "No key uri");
  NS_ABORT_IF_FALSE(aRequest, "No request");
  NS_ABORT_IF_FALSE(aChannel, "No channel");

  mProperties = do_CreateInstance("@mozilla.org/properties;1");
  if (!mProperties)
    return NS_ERROR_OUT_OF_MEMORY;


  mURI = aURI;
  mKeyURI = aKeyURI;
  mRequest = aRequest;
  mChannel = aChannel;
  mChannel->GetNotificationCallbacks(getter_AddRefs(mPrevChannelSink));

  NS_ASSERTION(mPrevChannelSink != this,
               "Initializing with a channel that already calls back to us!");

  mChannel->SetNotificationCallbacks(this);

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

void imgRequest::SetCacheEntry(imgCacheEntry *entry)
{
  mCacheEntry = entry;
}

PRBool imgRequest::HasCacheEntry() const
{
  return mCacheEntry != nsnull;
}

nsresult imgRequest::AddProxy(imgRequestProxy *proxy)
{
  NS_PRECONDITION(proxy, "null imgRequestProxy passed in");
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::AddProxy", "proxy", proxy);

  // If we're empty before adding, we have to tell the loader we now have
  // proxies.
  if (mObservers.IsEmpty()) {
    NS_ABORT_IF_FALSE(mKeyURI, "Trying to SetHasProxies without key uri.");
    imgLoader::SetHasProxies(mKeyURI);
  }

  return mObservers.AppendElementUnlessExists(proxy) ?
    NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult imgRequest::RemoveProxy(imgRequestProxy *proxy, nsresult aStatus, PRBool aNotify)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy", "proxy", proxy);

  mObservers.RemoveElement(proxy);

  /* Check mState below before we potentially call Cancel() below. Since
     Cancel() may result in OnStopRequest being called back before Cancel()
     returns, leaving mState in a different state then the one it was in at
     this point.
   */

  if (aNotify) {
    // make sure that observer gets an OnStopDecode message sent to it
    if (!(mState & stateRequestStopped)) {
      proxy->OnStopDecode(aStatus, nsnull);
    }

  }

  // make sure that observer gets an OnStopRequest message sent to it
  if (!(mState & stateRequestStopped)) {
    proxy->OnStopRequest(nsnull, nsnull, NS_BINDING_ABORTED, PR_TRUE);
  }

  if (mImage && !HaveProxyWithObserver(nsnull)) {
    LOG_MSG(gImgLog, "imgRequest::RemoveProxy", "stopping animation");

    mImage->StopAnimation();
  }

  if (mObservers.IsEmpty()) {
    // If we have no observers, there's nothing holding us alive. If we haven't
    // been cancelled and thus removed from the cache, tell the image loader so
    // we can be evicted from the cache.
    if (mCacheEntry) {
      NS_ABORT_IF_FALSE(mKeyURI, "Removing last observer without key uri.");

      imgLoader::SetHasNoProxies(mKeyURI, mCacheEntry);
    } 
#if defined(PR_LOGGING)
    else {
      nsCAutoString spec;
      mKeyURI->GetSpec(spec);
      LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy no cache entry", "uri", spec.get());
    }
#endif

    /* If |aStatus| is a failure code, then cancel the load if it is still in progress.
       Otherwise, let the load continue, keeping 'this' in the cache with no observers.
       This way, if a proxy is destroyed without calling cancel on it, it won't leak
       and won't leave a bad pointer in mObservers.
     */
    if (mRequest && mLoading && NS_FAILED(aStatus)) {
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
    proxy->RemoveFromLoadGroup(PR_TRUE);

  return NS_OK;
}

nsresult imgRequest::NotifyProxyListener(imgRequestProxy *proxy)
{
  nsCOMPtr<imgIRequest> kungFuDeathGrip(proxy);

  // OnStartRequest
  if (mState & stateRequestStarted)
    proxy->OnStartRequest(nsnull, nsnull);

  // OnStartContainer
  if (mState & stateHasSize)
    proxy->OnStartContainer(mImage);

  // OnStartDecode
  if (mState & stateDecodeStarted)
    proxy->OnStartDecode();

  // Send frame messages (OnStartFrame, OnDataAvailable, OnStopFrame)
  PRUint32 nframes = 0;
  if (mImage)
    mImage->GetNumFrames(&nframes);

  if (nframes > 0) {
    PRUint32 frame;
    mImage->GetCurrentFrameIndex(&frame);
    proxy->OnStartFrame(frame);

    // OnDataAvailable
    // XXX - Should only send partial rects here, but that needs to
    // wait until we fix up the observer interface
    nsIntRect r;
    mImage->GetCurrentFrameRect(r);
    proxy->OnDataAvailable(frame, &r);

    if (mState & stateRequestStopped)
      proxy->OnStopFrame(frame);
  }

  if (mImage && !HaveProxyWithObserver(proxy) && proxy->HasObserver()) {
    LOG_MSG(gImgLog, "imgRequest::NotifyProxyListener", "resetting animation");

    mImage->ResetAnimation();
  }

  if (mState & stateRequestStopped) {
    proxy->OnStopContainer(mImage);
    proxy->OnStopDecode(GetResultFromImageStatus(mImageStatus), nsnull);
    proxy->OnStopRequest(nsnull, nsnull,
                         GetResultFromImageStatus(mImageStatus),
                         mHadLastPart);
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

  if (mRequest && mLoading)
    mRequest->Cancel(aStatus);
}

void imgRequest::CancelAndAbort(nsresult aStatus)
{
  LOG_SCOPE(gImgLog, "imgRequest::CancelAndAbort");

  Cancel(aStatus);

  // It's possible for the channel to fail to open after we've set our
  // notification callbacks. In that case, make sure to break the cycle between
  // the channel and us, because it won't.
  if (mChannel) {
    mChannel->SetNotificationCallbacks(mPrevChannelSink);
    mPrevChannelSink = nsnull;
  }
}

nsresult imgRequest::GetURI(nsIURI **aURI)
{
  LOG_FUNC(gImgLog, "imgRequest::GetURI");

  if (mURI) {
    *aURI = mURI;
    NS_ADDREF(*aURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult imgRequest::GetKeyURI(nsIURI **aKeyURI)
{
  LOG_FUNC(gImgLog, "imgRequest::GetKeyURI");

  if (mKeyURI) {
    *aKeyURI = mKeyURI;
    NS_ADDREF(*aKeyURI);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult imgRequest::GetPrincipal(nsIPrincipal **aPrincipal)
{
  LOG_FUNC(gImgLog, "imgRequest::GetPrincipal");

  if (mPrincipal) {
    NS_ADDREF(*aPrincipal = mPrincipal);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

nsresult imgRequest::GetSecurityInfo(nsISupports **aSecurityInfo)
{
  LOG_FUNC(gImgLog, "imgRequest::GetSecurityInfo");

  // Missing security info means this is not a security load
  // i.e. it is not an error when security info is missing
  NS_IF_ADDREF(*aSecurityInfo = mSecurityInfo);
  return NS_OK;
}

void imgRequest::RemoveFromCache()
{
  LOG_SCOPE(gImgLog, "imgRequest::RemoveFromCache");

  if (mIsInCache) {
    // mCacheEntry is nulled out when we have no more observers.
    if (mCacheEntry)
      imgLoader::RemoveFromCache(mCacheEntry);
    else
      imgLoader::RemoveFromCache(mKeyURI);
  }

  mCacheEntry = nsnull;
}

PRBool imgRequest::HaveProxyWithObserver(imgRequestProxy* aProxyToIgnore) const
{
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  imgRequestProxy* proxy;
  while (iter.HasMore()) {
    proxy = iter.GetNext();
    if (proxy == aProxyToIgnore) {
      continue;
    }
    
    if (proxy->HasObserver()) {
      return PR_TRUE;
    }
  }
  
  return PR_FALSE;
}

PRInt32 imgRequest::Priority() const
{
  PRInt32 priority = nsISupportsPriority::PRIORITY_NORMAL;
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mRequest);
  if (p)
    p->GetPriority(&priority);
  return priority;
}

void imgRequest::AdjustPriority(imgRequestProxy *proxy, PRInt32 delta)
{
  // only the first proxy is allowed to modify the priority of this image load.
  //
  // XXX(darin): this is probably not the most optimal algorithm as we may want
  // to increase the priority of requests that have a lot of proxies.  the key
  // concern though is that image loads remain lower priority than other pieces
  // of content such as link clicks, CSS, and JS.
  //
  if (mObservers.SafeElementAt(0, nsnull) != proxy)
    return;

  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(mRequest);
  if (p)
    p->AdjustPriority(delta);
}

void imgRequest::SetIsInCache(PRBool incache)
{
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::SetIsCacheable", "incache", incache);
  mIsInCache = incache;
}

void imgRequest::UpdateCacheEntrySize()
{
  if (mCacheEntry) {
    PRUint32 imageSize = 0;
    if (mImage)
      mImage->GetDataSize(&imageSize);
    mCacheEntry->SetDataSize(imageSize);

#ifdef DEBUG_joe
    nsCAutoString url;
    mURI->GetSpec(url);
    printf("CACHEPUT: %d %s %d\n", time(NULL), url.get(), imageSize);
#endif
  }

}

nsresult
imgRequest::GetImage(imgIContainer **aImage)
{
  LOG_FUNC(gImgLog, "imgRequest::GetImage");

  *aImage = mImage;
  NS_IF_ADDREF(*aImage);
  return NS_OK;
}

nsresult
imgRequest::LockImage()
{
  // If we have an image, apply the lock directly
  if (mImage) {
    NS_ABORT_IF_FALSE(mDeferredLocks == 0, "Have image, but deferred locks?");
    return mImage->LockImage();
  }

  // Otherwise, queue it up for when we have an image
  mDeferredLocks++;

  return NS_OK;
}

nsresult
imgRequest::UnlockImage()
{
  // If we have an image, apply the unlock directly
  if (mImage) {
    NS_ABORT_IF_FALSE(mDeferredLocks == 0, "Have image, but deferred locks?");
    return mImage->UnlockImage();
  }

  // If we don't have an image, get rid of one of our deferred locks (we should
  // have one).
  NS_ABORT_IF_FALSE(mDeferredLocks > 0, "lock/unlock calls must be matched!");
  mDeferredLocks--;

  return NS_OK;
}

nsresult
imgRequest::RequestDecode()
{
  // If we have an image, apply the request directly
  if (mImage) {
    return mImage->RequestDecode();
  }

  // Otherwise, flag to do it when we get the image
  mDecodeRequested = PR_TRUE;

  return NS_OK;
}

/** imgIContainerObserver methods **/

/* [noscript] void frameChanged (in imgIContainer container, in nsIntRect dirtyRect); */
NS_IMETHODIMP imgRequest::FrameChanged(imgIContainer *container,
                                       nsIntRect * dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->FrameChanged(container, dirtyRect);
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");

  mState |= stateDecodeStarted;

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnStartDecode();
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

NS_IMETHODIMP imgRequest::OnStartRequest(imgIRequest *aRequest)
{
  NS_NOTREACHED("imgRequest(imgIDecoderObserver)::OnStartRequest");
  return NS_OK;
}

/* void onStartContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStartContainer(imgIRequest *request, imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartContainer");

  NS_ASSERTION(image, "imgRequest::OnStartContainer called with a null image!");
  if (!image) return NS_ERROR_UNEXPECTED;

  // We only want to send onStartContainer once
  PRBool alreadySent = (mState & stateHasSize) != 0;

  mState |= stateHasSize;

  mImageStatus |= imgIRequest::STATUS_SIZE_AVAILABLE;

  if (!alreadySent) {
    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
    while (iter.HasMore()) {
      iter.GetNext()->OnStartContainer(image);
    }
  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request,
                                       PRUint32 frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnStartFrame(frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in boolean aCurrentFrame, [const] in nsIntRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request,
                                          PRBool aCurrentFrame,
                                          const nsIntRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnDataAvailable(aCurrentFrame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request,
                                      PRUint32 frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");

  mImageStatus |= imgIRequest::STATUS_FRAME_COMPLETE;

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnStopFrame(frame);
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request,
                                          imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnStopContainer(image);
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *aRequest,
                                       nsresult aStatus,
                                       const PRUnichar *aStatusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");

  // We finished the decode, and thus have the decoded frames. Update the cache
  // entry size to take this into account.
  UpdateCacheEntrySize();

  // If we were successful, set STATUS_DECODE_COMPLETE
  if (NS_SUCCEEDED(aStatus))
    mImageStatus |= imgIRequest::STATUS_DECODE_COMPLETE;

  // ImgContainer and everything below it is completely correct and
  // bulletproof about its handling of decoder notifications.
  // Unfortunately, here and above we have to make some gross and
  // inappropriate use of things to get things to work without
  // completely overhauling the decoder observer interface (this will,
  // thankfully, happen in bug 505385). From imgRequest and above (for
  // the time being), OnStopDecode is just a companion to OnStopRequest
  // that signals success or failure of the _load_ (not the _decode_).
  // As such, we ignore OnStopDecode notifications from the decoder and
  // container and generate our own every time we send OnStopRequest.
  // For more information, see bug 435296.

  return NS_OK;
}

NS_IMETHODIMP imgRequest::OnStopRequest(imgIRequest *aRequest,
                                        PRBool aLastPart)
{
  NS_NOTREACHED("imgRequest(imgIDecoderObserver)::OnStopRequest");
  return NS_OK;
}

/* void onDiscard (in imgIRequest request); */
NS_IMETHODIMP imgRequest::OnDiscard(imgIRequest *aRequest)
{
  // Clear the state bits we no longer deserve.
  PRUint32 stateBitsToClear = stateDecodeStarted;
  mState &= ~stateBitsToClear;

  // Clear the status bits we no longer deserve.
  PRUint32 statusBitsToClear = imgIRequest::STATUS_FRAME_COMPLETE
                               | imgIRequest::STATUS_DECODE_COMPLETE;
  mImageStatus &= ~statusBitsToClear;

  // Update the cache entry size, since we just got rid of frame data
  UpdateCacheEntrySize();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnDiscard();
  }

  return NS_OK;

}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  nsresult rv;

  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  // Figure out if we're multipart
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan)
      mIsMultiPartChannel = PR_TRUE;

  // If we're not multipart, we shouldn't have an image yet
  NS_ABORT_IF_FALSE(mIsMultiPartChannel || !mImage,
                    "Already have an image for non-multipart request");

  // If we're multipart and have an image, fix things up for another round
  if (mIsMultiPartChannel && mImage) {

    // Inform the container that we have new source data
    mImage->NewSourceData();

    // Clear any status and state bits indicating load/decode
    mImageStatus &= ~imgIRequest::STATUS_LOAD_PARTIAL;
    mImageStatus &= ~imgIRequest::STATUS_LOAD_COMPLETE;
    mImageStatus &= ~imgIRequest::STATUS_FRAME_COMPLETE;
    mState &= ~stateRequestStarted;
    mState &= ~stateDecodeStarted;
    mState &= ~stateRequestStopped;
  }

  /*
   * If mRequest is null here, then we need to set it so that we'll be able to
   * cancel it if our Cancel() method is called.  Note that this can only
   * happen for multipart channels.  We could simply not null out mRequest for
   * non-last parts, if GetIsLastPart() were reliable, but it's not.  See
   * https://bugzilla.mozilla.org/show_bug.cgi?id=339610
   */
  if (!mRequest) {
    NS_ASSERTION(mpchan,
                 "We should have an mRequest here unless we're multipart");
    nsCOMPtr<nsIChannel> chan;
    mpchan->GetBaseChannel(getter_AddRefs(chan));
    mRequest = chan;
  }

  // The request has started
  mState |= stateRequestStarted;

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
    channel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  /* set our loading flag to true */
  mLoading = PR_TRUE;

  /* notify our kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    iter.GetNext()->OnStartRequest(aRequest, ctxt);
  }

  /* Get our principal */
  nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
  if (chan) {
    nsCOMPtr<nsIScriptSecurityManager> secMan =
      do_GetService("@mozilla.org/scriptsecuritymanager;1");
    if (secMan) {
      nsresult rv = secMan->GetChannelPrincipal(chan,
                                                getter_AddRefs(mPrincipal));
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  /* get the expires info */
  if (mCacheEntry) {
    nsCOMPtr<nsICachingChannel> cacheChannel(do_QueryInterface(aRequest));
    if (cacheChannel) {
      nsCOMPtr<nsISupports> cacheToken;
      cacheChannel->GetCacheToken(getter_AddRefs(cacheToken));
      if (cacheToken) {
        nsCOMPtr<nsICacheEntryInfo> entryDesc(do_QueryInterface(cacheToken));
        if (entryDesc) {
          PRUint32 expiration;
          /* get the expiration time from the caching channel's token */
          entryDesc->GetExpirationTime(&expiration);

          /* set the expiration time on our entry */
          mCacheEntry->SetExpiryTime(expiration);
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

      mCacheEntry->SetMustValidateIfExpired(bMustRevalidate);
    }
  }


  // Shouldn't we be dead already if this gets hit?  Probably multipart/x-mixed-replace...
  if (mObservers.IsEmpty()) {
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
  }

  return NS_OK;
}

/* void onStopRequest (in nsIRequest request, in nsISupports ctxt, in nsresult status); */
NS_IMETHODIMP imgRequest::OnStopRequest(nsIRequest *aRequest, nsISupports *ctxt, nsresult status)
{
  LOG_FUNC(gImgLog, "imgRequest::OnStopRequest");

  mState |= stateRequestStopped;

  /* set our loading flag to false */
  mLoading = PR_FALSE;

  mHadLastPart = PR_TRUE;
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan) {
    PRBool lastPart;
    nsresult rv = mpchan->GetIsLastPart(&lastPart);
    if (NS_SUCCEEDED(rv))
      mHadLastPart = lastPart;
  }

  // XXXldb What if this is a non-last part of a multipart request?
  // xxx before we release our reference to mRequest, lets
  // save the last status that we saw so that the
  // imgRequestProxy will have access to it.
  if (mRequest) {
    mRequest = nsnull;  // we no longer need the request
  }

  // stop holding a ref to the channel, since we don't need it anymore
  if (mChannel) {
    mChannel->SetNotificationCallbacks(mPrevChannelSink);
    mPrevChannelSink = nsnull;
    mChannel = nsnull;
  }

  // Tell the image that it has all of the source data. Note that this can
  // trigger a failure, since the image might be waiting for more non-optional
  // data and this is the point where we break the news that it's not coming.
  if (mImage) {

    // Notify the image
    nsresult rv = mImage->SourceDataComplete();

    // If we got an error in the SourceDataComplete() call, we don't want to
    // proceed as if nothing bad happened. However, we also want to give
    // precedence to failure status codes from necko, since presumably
    // they're more meaningful.
    if (NS_FAILED(rv) && NS_SUCCEEDED(status))
      status = rv;
  }

  // If the request went through, say we loaded the image, and update the
  // cache entry size. Otherwise, cancel the request, which adds an error
  // flag to mImageStatus.
  if (NS_SUCCEEDED(status)) {

    // Flag that we loaded the image
    mImageStatus |= imgIRequest::STATUS_LOAD_COMPLETE;

    // We update the cache entry size here because this is where we finish
    // loading compressed source data, which is part of our size calculus.
    UpdateCacheEntrySize();
  }
  else
    this->Cancel(status); // sets status, stops animations, removes from cache

  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator sdIter(mObservers);
  while (sdIter.HasMore()) {
    sdIter.GetNext()->OnStopDecode(GetResultFromImageStatus(mImageStatus), nsnull);
  }

  nsTObserverArray<imgRequestProxy*>::ForwardIterator srIter(mObservers);
  while (srIter.HasMore()) {
    srIter.GetNext()->OnStopRequest(aRequest, ctxt, status, mHadLastPart);
  }

  return NS_OK;
}

/* prototype for these defined below */
static NS_METHOD sniff_mimetype_callback(nsIInputStream* in, void* closure, const char* fromRawSegment,
                                         PRUint32 toOffset, PRUint32 count, PRUint32 *writeCount);

/** nsIStreamListener methods **/

/* void onDataAvailable (in nsIRequest request, in nsISupports ctxt, in nsIInputStream inStr, in unsigned long sourceOffset, in unsigned long count); */
NS_IMETHODIMP imgRequest::OnDataAvailable(nsIRequest *aRequest, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "count", count);

  NS_ASSERTION(aRequest, "imgRequest::OnDataAvailable -- no request!");

  mGotData = PR_TRUE;
  nsresult rv;

  if (!mImage) {
    LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |First time through... finding mimetype|");

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

      rv = NS_ERROR_FAILURE;
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

    /* set our mimetype as a property */
    nsCOMPtr<nsISupportsCString> contentType(do_CreateInstance("@mozilla.org/supports-cstring;1"));
    if (contentType) {
      contentType->SetData(mContentType);
      mProperties->Set("type", contentType);
    }

    /* set our content disposition as a property */
    nsCAutoString disposition;
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
    if (httpChannel) {
      httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-disposition"), disposition);
    } else {
      nsCOMPtr<nsIMultiPartChannel> multiPartChannel(do_QueryInterface(aRequest));
      if (multiPartChannel) {
        multiPartChannel->GetContentDisposition(disposition);
      }
    }
    if (!disposition.IsEmpty()) {
      nsCOMPtr<nsISupportsCString> contentDisposition(do_CreateInstance("@mozilla.org/supports-cstring;1"));
      if (contentDisposition) {
        contentDisposition->SetData(disposition);
        mProperties->Set("content-disposition", contentDisposition);
      }
    }

    LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnDataAvailable", "content type", mContentType.get());

    //
    // Figure out if our container initialization flags
    //

    // We default to the static globals
    PRBool isDiscardable = gDiscardable;
    PRBool doDecodeOnDraw = gDecodeOnDraw;

    // We want UI to be as snappy as possible and not to flicker. Disable discarding
    // and decode-on-draw for chrome URLS
    PRBool isChrome = PR_FALSE;
    rv = mURI->SchemeIs("chrome", &isChrome);
    if (NS_SUCCEEDED(rv) && isChrome)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // We don't want resources like the "loading" icon to be discardable or
    // decode-on-draw either.
    PRBool isResource = PR_FALSE;
    rv = mURI->SchemeIs("resource", &isResource);
    if (NS_SUCCEEDED(rv) && isResource)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // For multipart/x-mixed-replace, we basically want a direct channel to the
    // decoder. Disable both for this case as well.
    if (mIsMultiPartChannel)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // We have all the information we need
    PRUint32 containerFlags = imgIContainer::INIT_FLAG_NONE;
    if (isDiscardable)
      containerFlags |= imgIContainer::INIT_FLAG_DISCARDABLE;
    if (doDecodeOnDraw)
      containerFlags |= imgIContainer::INIT_FLAG_DECODE_ON_DRAW;
    if (mIsMultiPartChannel)
      containerFlags |= imgIContainer::INIT_FLAG_MULTIPART;

    // Create and initialize the imgContainer. This instantiates a decoder behind
    // the scenes, so if we don't have a decoder for this mimetype we'll find out
    // about it here.
    mImage = do_CreateInstance("@mozilla.org/image/container;3");
    if (!mImage) {
      this->Cancel(NS_ERROR_OUT_OF_MEMORY);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    rv = mImage->Init(this, mContentType.get(), containerFlags);
    if (NS_FAILED(rv)) { // Probably bad mimetype

      // There's no reason to keep the image around. Save memory.
      //
      // XXXbholley - This is also here because I'm not sure we've found
      // all the consumers who (incorrectly) check whether the container
      // is null to determine things like size availability (they should
      // be checking the image status instead).
      mImage = nsnull;

      this->Cancel(rv);
      return NS_BINDING_ABORTED;
    }

    // If we were waiting on the image to do something, now's our chance.
    if (mDecodeRequested) {
      mImage->RequestDecode();
    }
    while (mDeferredLocks) {
      mImage->LockImage();
      mDeferredLocks--;
    }
  }

  // WriteToContainer always consumes everything it gets
  PRUint32 bytesRead;
  rv = inStr->ReadSegments(imgContainer::WriteToContainer,
                           static_cast<void*>(mImage),
                           count, &bytesRead);
  if (NS_FAILED(rv)) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- "
            "copy to container failed\n", this));
    this->Cancel(NS_IMAGELIB_ERROR_FAILURE);
    return NS_BINDING_ABORTED;
  }
  NS_ABORT_IF_FALSE(bytesRead == count, "WriteToContainer should consume everything!");

  return NS_OK;
}

static NS_METHOD sniff_mimetype_callback(nsIInputStream* in,
                                         void* closure,
                                         const char* fromRawSegment,
                                         PRUint32 toOffset,
                                         PRUint32 count,
                                         PRUint32 *writeCount)
{
  imgRequest *request = static_cast<imgRequest*>(closure);

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

  // The vast majority of the time, imgLoader will find a gif/jpeg/png image
  // and fill mContentType with the sniffed MIME type.
  if (!mContentType.IsEmpty())
    return;

  // When our sniffing fails, we want to query registered image decoders
  // to see if they can identify the image. If we always trusted the server
  // to send the right MIME, images sent as text/plain would not be rendered.
  const nsCOMArray<nsIContentSniffer>& sniffers = mImageSniffers.GetEntries();
  PRUint32 length = sniffers.Count();
  for (PRUint32 i = 0; i < length; ++i) {
    nsresult rv =
      sniffers[i]->GetMIMETypeFromContent(nsnull, (const PRUint8 *) buf, len, mContentType);
    if (NS_SUCCEEDED(rv) && !mContentType.IsEmpty()) {
      return;
    }
  }
}


/** nsIInterfaceRequestor methods **/

NS_IMETHODIMP
imgRequest::GetInterface(const nsIID & aIID, void **aResult)
{
  if (!mPrevChannelSink || aIID.Equals(NS_GET_IID(nsIChannelEventSink)))
    return QueryInterface(aIID, aResult);

  NS_ASSERTION(mPrevChannelSink != this, 
               "Infinite recursion - don't keep track of channel sinks that are us!");
  return mPrevChannelSink->GetInterface(aIID, aResult);
}

/** nsIChannelEventSink methods **/

/* void onChannelRedirect (in nsIChannel oldChannel, in nsIChannel newChannel, in unsigned long flags); */
NS_IMETHODIMP
imgRequest::OnChannelRedirect(nsIChannel *oldChannel, nsIChannel *newChannel, PRUint32 flags)
{
  NS_ASSERTION(mRequest && mChannel, "Got an OnChannelRedirect after we nulled out mRequest!");
  NS_ASSERTION(mChannel == oldChannel, "Got a channel redirect for an unknown channel!");
  NS_ASSERTION(newChannel, "Got a redirect to a NULL channel!");

  nsresult rv = NS_OK;
  nsCOMPtr<nsIChannelEventSink> sink(do_GetInterface(mPrevChannelSink));
  if (sink) {
    rv = sink->OnChannelRedirect(oldChannel, newChannel, flags);
    if (NS_FAILED(rv))
      return rv;
  }

  mChannel = newChannel;

  // Don't make any cache changes if we're going to point to the same thing. We
  // compare specs and not just URIs here because URIs that compare as
  // .Equals() might have different hashes.
  nsCAutoString oldspec;
  if (mKeyURI)
    mKeyURI->GetSpec(oldspec);
  LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnChannelRedirect", "old", oldspec.get());

  nsCOMPtr<nsIURI> newURI;
  newChannel->GetOriginalURI(getter_AddRefs(newURI));
  nsCAutoString newspec;
  if (newURI)
    newURI->GetSpec(newspec);
  LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnChannelRedirect", "new", newspec.get());

  if (oldspec != newspec) {
    if (mIsInCache) {
      // Remove the cache entry from the cache, but don't null out mCacheEntry
      // (as imgRequest::RemoveFromCache() does), because we need it to put
      // ourselves back in the cache.
      if (mCacheEntry)
        imgLoader::RemoveFromCache(mCacheEntry);
      else
        imgLoader::RemoveFromCache(mKeyURI);
    }

    mKeyURI = newURI;
 
    if (mIsInCache) {
      // If we don't still have a URI or cache entry, we don't want to put
      // ourselves back into the cache.
      if (mKeyURI && mCacheEntry)
        imgLoader::PutIntoCache(mKeyURI, mCacheEntry);
    }
  }

  return rv;
}
