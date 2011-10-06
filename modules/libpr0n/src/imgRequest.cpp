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
#include "ImageLogging.h"

/* We end up pulling in windows.h because we eventually hit gfxWindowsSurface;
 * windows.h defines LoadImage, so we have to #undef it or imgLoader::LoadImage
 * gets changed.
 * This #undef needs to be in multiple places because we don't always pull
 * headers in in the same order.
 */
#undef LoadImage

#include "imgLoader.h"
#include "imgRequestProxy.h"
#include "RasterImage.h"
#include "VectorImage.h"

#include "imgILoader.h"

#include "netCore.h"

#include "nsIChannel.h"
#include "nsICachingChannel.h"
#include "nsILoadGroup.h"
#include "nsIInputStream.h"
#include "nsIMultiPartChannel.h"
#include "nsIHttpChannel.h"

#include "nsIComponentManager.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsIScriptSecurityManager.h"

#include "nsICacheVisitor.h"

#include "nsString.h"
#include "nsXPIDLString.h"
#include "plstr.h" // PL_strcasestr(...)
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"

#include "mozilla/Preferences.h"

#include "DiscardTracker.h"
#include "nsAsyncRedirectVerifyHelper.h"

#define DISCARD_PREF "image.mem.discardable"
#define DECODEONDRAW_PREF "image.mem.decodeondraw"
#define BYTESATATIME_PREF "image.mem.decode_bytes_at_a_time"
#define MAXMS_PREF "image.mem.max_ms_before_yield"
#define MAXBYTESFORSYNC_PREF "image.mem.max_bytes_for_sync_decode"
#define SVG_MIMETYPE "image/svg+xml"

using namespace mozilla;
using namespace mozilla::imagelib;

/* Kept up to date by a pref observer. */
static bool gDecodeOnDraw = false;
static bool gDiscardable = false;

static const char* kObservedPrefs[] = {
  DISCARD_PREF,
  DECODEONDRAW_PREF,
  DISCARD_TIMEOUT_PREF,
  nsnull
};

/*
 * Pref observer goop. Yuck.
 */

// Flag
static bool gRegisteredPrefObserver = false;

// Reloader
static void
ReloadPrefs()
{
  // Discardable
  gDiscardable = Preferences::GetBool(DISCARD_PREF, gDiscardable);

  // Decode-on-draw
  gDecodeOnDraw = Preferences::GetBool(DECODEONDRAW_PREF, gDecodeOnDraw);

  // Progressive decoding knobs
  PRInt32 bytesAtATime, maxMS, maxBytesForSync;
  if (NS_SUCCEEDED(Preferences::GetInt(BYTESATATIME_PREF, &bytesAtATime))) {
    RasterImage::SetDecodeBytesAtATime(bytesAtATime);
  }

  if (NS_SUCCEEDED(Preferences::GetInt(MAXMS_PREF, &maxMS))) {
    RasterImage::SetMaxMSBeforeYield(maxMS);
  }

  if (NS_SUCCEEDED(Preferences::GetInt(MAXBYTESFORSYNC_PREF,
                                       &maxBytesForSync))) {
    RasterImage::SetMaxBytesForSyncDecode(maxBytesForSync);
  }

  // Discard timeout
  mozilla::imagelib::DiscardTracker::ReloadTimeout();
}

// Observer
class imgRequestPrefObserver : public nsIObserver {
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
};
NS_IMPL_ISUPPORTS1(imgRequestPrefObserver, nsIObserver)

// Callback
NS_IMETHODIMP
imgRequestPrefObserver::Observe(nsISupports     *aSubject,
                                const char      *aTopic,
                                const PRUnichar *aData)
{
  // Right topic
  NS_ABORT_IF_FALSE(!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID), "invalid topic");

  // Right pref
  if (strcmp(NS_LossyConvertUTF16toASCII(aData).get(), DISCARD_PREF) &&
      strcmp(NS_LossyConvertUTF16toASCII(aData).get(), DECODEONDRAW_PREF) &&
      strcmp(NS_LossyConvertUTF16toASCII(aData).get(), DISCARD_TIMEOUT_PREF))
    return NS_OK;

  // Process the change
  ReloadPrefs();

  return NS_OK;
}

#if defined(PR_LOGGING)
PRLogModuleInfo *gImgLog = PR_NewLogModule("imgRequest");
#endif

NS_IMPL_ISUPPORTS8(imgRequest,
                   imgIDecoderObserver, imgIContainerObserver,
                   nsIStreamListener, nsIRequestObserver,
                   nsISupportsWeakReference,
                   nsIChannelEventSink,
                   nsIInterfaceRequestor,
                   nsIAsyncVerifyRedirectCallback)

imgRequest::imgRequest() : 
  mValidator(nsnull), mImageSniffers("image-sniffing-services"),
  mInnerWindowId(0), mCORSMode(imgIRequest::CORS_NONE),
  mDecodeRequested(PR_FALSE), mIsMultiPartChannel(PR_FALSE), mGotData(PR_FALSE),
  mIsInCache(PR_FALSE)
{}

imgRequest::~imgRequest()
{
  if (mURI) {
    nsCAutoString spec;
    mURI->GetSpec(spec);
    LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::~imgRequest()", "keyuri", spec.get());
  } else
    LOG_FUNC(gImgLog, "imgRequest::~imgRequest()");
}

nsresult imgRequest::Init(nsIURI *aURI,
                          nsIURI *aCurrentURI,
                          nsIRequest *aRequest,
                          nsIChannel *aChannel,
                          imgCacheEntry *aCacheEntry,
                          void *aLoadId,
                          nsIPrincipal* aLoadingPrincipal,
                          PRInt32 aCORSMode)
{
  LOG_FUNC(gImgLog, "imgRequest::Init");

  NS_ABORT_IF_FALSE(!mImage, "Multiple calls to init");
  NS_ABORT_IF_FALSE(aURI, "No uri");
  NS_ABORT_IF_FALSE(aCurrentURI, "No current uri");
  NS_ABORT_IF_FALSE(aRequest, "No request");
  NS_ABORT_IF_FALSE(aChannel, "No channel");

  mProperties = do_CreateInstance("@mozilla.org/properties;1");

  mStatusTracker = new imgStatusTracker(nsnull);

  mURI = aURI;
  mCurrentURI = aCurrentURI;
  mRequest = aRequest;
  mChannel = aChannel;
  mTimedChannel = do_QueryInterface(mChannel);

  mLoadingPrincipal = aLoadingPrincipal;
  mCORSMode = aCORSMode;

  mChannel->GetNotificationCallbacks(getter_AddRefs(mPrevChannelSink));

  NS_ASSERTION(mPrevChannelSink != this,
               "Initializing with a channel that already calls back to us!");

  mChannel->SetNotificationCallbacks(this);

  mCacheEntry = aCacheEntry;

  SetLoadId(aLoadId);

  // Register our pref observer if it hasn't been done yet.
  if (NS_UNLIKELY(!gRegisteredPrefObserver)) {
    nsCOMPtr<nsIObserver> observer(new imgRequestPrefObserver());
    Preferences::AddStrongObservers(observer, kObservedPrefs);
    ReloadPrefs();
    gRegisteredPrefObserver = PR_TRUE;
  }

  return NS_OK;
}

imgStatusTracker&
imgRequest::GetStatusTracker()
{
  if (mImage) {
    NS_ABORT_IF_FALSE(!mStatusTracker,
                      "Should have given mStatusTracker to mImage");
    return mImage->GetStatusTracker();
  } else {
    NS_ABORT_IF_FALSE(mStatusTracker,
                      "Should have mStatusTracker until we create mImage");
    return *mStatusTracker;
  }
}

void imgRequest::SetCacheEntry(imgCacheEntry *entry)
{
  mCacheEntry = entry;
}

bool imgRequest::HasCacheEntry() const
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
    NS_ABORT_IF_FALSE(mURI, "Trying to SetHasProxies without key uri.");
    imgLoader::SetHasProxies(mURI);
  }

  // If we don't have any current observers, we should restart any animation.
  if (mImage && !HaveProxyWithObserver(proxy) && proxy->HasObserver()) {
    LOG_MSG(gImgLog, "imgRequest::AddProxy", "resetting animation");

    mImage->ResetAnimation();
  }

  proxy->SetPrincipal(mPrincipal);

  return mObservers.AppendElementUnlessExists(proxy) ?
    NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

nsresult imgRequest::RemoveProxy(imgRequestProxy *proxy, nsresult aStatus, bool aNotify)
{
  LOG_SCOPE_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy", "proxy", proxy);

  // This will remove our animation consumers, so after removing
  // this proxy, we don't end up without proxies with observers, but still
  // have animation consumers.
  proxy->ClearAnimationConsumers();

  mObservers.RemoveElement(proxy);

  // Let the status tracker do its thing before we potentially call Cancel()
  // below, because Cancel() may result in OnStopRequest being called back
  // before Cancel() returns, leaving the image in a different state then the
  // one it was in at this point.

  imgStatusTracker& statusTracker = GetStatusTracker();
  statusTracker.EmulateRequestFinished(proxy, aStatus, !aNotify);

  if (mObservers.IsEmpty()) {
    // If we have no observers, there's nothing holding us alive. If we haven't
    // been cancelled and thus removed from the cache, tell the image loader so
    // we can be evicted from the cache.
    if (mCacheEntry) {
      NS_ABORT_IF_FALSE(mURI, "Removing last observer without key uri.");

      imgLoader::SetHasNoProxies(mURI, mCacheEntry);
    } 
#if defined(PR_LOGGING)
    else {
      nsCAutoString spec;
      mURI->GetSpec(spec);
      LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::RemoveProxy no cache entry", "uri", spec.get());
    }
#endif

    /* If |aStatus| is a failure code, then cancel the load if it is still in progress.
       Otherwise, let the load continue, keeping 'this' in the cache with no observers.
       This way, if a proxy is destroyed without calling cancel on it, it won't leak
       and won't leave a bad pointer in mObservers.
     */
    if (statusTracker.IsLoading() && NS_FAILED(aStatus)) {
      LOG_MSG(gImgLog, "imgRequest::RemoveProxy", "load in progress.  canceling");

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

void imgRequest::Cancel(nsresult aStatus)
{
  /* The Cancel() method here should only be called by this class. */

  LOG_SCOPE(gImgLog, "imgRequest::Cancel");

  imgStatusTracker& statusTracker = GetStatusTracker();
  statusTracker.RecordCancel();

  RemoveFromCache();

  if (mRequest && statusTracker.IsLoading())
    mRequest->Cancel(aStatus);
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
      imgLoader::RemoveFromCache(mURI);
  }

  mCacheEntry = nsnull;
}

bool imgRequest::HaveProxyWithObserver(imgRequestProxy* aProxyToIgnore) const
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

void imgRequest::SetIsInCache(bool incache)
{
  LOG_FUNC_WITH_PARAM(gImgLog, "imgRequest::SetIsCacheable", "incache", incache);
  mIsInCache = incache;
}

void imgRequest::UpdateCacheEntrySize()
{
  if (mCacheEntry) {
    mCacheEntry->SetDataSize(mImage->GetDataSize());

#ifdef DEBUG_joe
    nsCAutoString url;
    mURI->GetSpec(url);
    printf("CACHEPUT: %d %s %d\n", time(NULL), url.get(), imageSize);
#endif
  }
}

void imgRequest::SetCacheValidation(imgCacheEntry* aCacheEntry, nsIRequest* aRequest)
{
  /* get the expires info */
  if (aCacheEntry) {
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

          // Expiration time defaults to 0. We set the expiration time on our
          // entry if it hasn't been set yet.
          if (aCacheEntry->GetExpiryTime() == 0)
            aCacheEntry->SetExpiryTime(expiration);
        }
      }
    }

    // Determine whether the cache entry must be revalidated when we try to use it.
    // Currently, only HTTP specifies this information...
    nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
    if (httpChannel) {
      bool bMustRevalidate = false;

      httpChannel->IsNoStoreResponse(&bMustRevalidate);

      if (!bMustRevalidate) {
        httpChannel->IsNoCacheResponse(&bMustRevalidate);
      }

      if (!bMustRevalidate) {
        nsCAutoString cacheHeader;

        httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("Cache-Control"),
                                            cacheHeader);
        if (PL_strcasestr(cacheHeader.get(), "must-revalidate")) {
          bMustRevalidate = PR_TRUE;
        }
      }

      // Cache entries default to not needing to validate. We ensure that
      // multiple calls to this function don't override an earlier decision to
      // validate by making validation a one-way decision.
      if (bMustRevalidate)
        aCacheEntry->SetMustValidate(bMustRevalidate);
    }

    // We always need to validate file URIs.
    nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
    if (channel) {
      nsCOMPtr<nsIURI> uri;
      channel->GetURI(getter_AddRefs(uri));
      bool isfile = false;
      uri->SchemeIs("file", &isfile);
      if (isfile)
        aCacheEntry->SetMustValidate(isfile);
    }
  }
}

nsresult
imgRequest::LockImage()
{
  return mImage->LockImage();
}

nsresult
imgRequest::UnlockImage()
{
  return mImage->UnlockImage();
}

nsresult
imgRequest::RequestDecode()
{
  // If we've initialized our image, we can request a decode.
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
                                       const nsIntRect *dirtyRect)
{
  LOG_SCOPE(gImgLog, "imgRequest::FrameChanged");
  NS_ABORT_IF_FALSE(mImage,
                    "FrameChanged callback before we've created our image");

  mImage->GetStatusTracker().RecordFrameChanged(container, dirtyRect);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendFrameChanged(iter.GetNext(), container, dirtyRect);
  }

  return NS_OK;
}

/** imgIDecoderObserver methods **/

/* void onStartDecode (in imgIRequest request); */
NS_IMETHODIMP imgRequest::OnStartDecode(imgIRequest *request)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartDecode");
  NS_ABORT_IF_FALSE(mImage,
                    "OnStartDecode callback before we've created our image");


  mImage->GetStatusTracker().RecordStartDecode();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStartDecode(iter.GetNext());
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

  NS_ABORT_IF_FALSE(mImage,
                    "OnStartContainer callback before we've created our image");
  NS_ABORT_IF_FALSE(image == mImage,
                    "OnStartContainer callback from an image we don't own");
  mImage->GetStatusTracker().RecordStartContainer(image);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStartContainer(iter.GetNext(), image);
  }

  return NS_OK;
}

/* void onStartFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgRequest::OnStartFrame(imgIRequest *request,
                                       PRUint32 frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartFrame");
  NS_ABORT_IF_FALSE(mImage,
                    "OnStartFrame callback before we've created our image");

  mImage->GetStatusTracker().RecordStartFrame(frame);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStartFrame(iter.GetNext(), frame);
  }

  return NS_OK;
}

/* [noscript] void onDataAvailable (in imgIRequest request, in boolean aCurrentFrame, [const] in nsIntRect rect); */
NS_IMETHODIMP imgRequest::OnDataAvailable(imgIRequest *request,
                                          bool aCurrentFrame,
                                          const nsIntRect * rect)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable");
  NS_ABORT_IF_FALSE(mImage,
                    "OnDataAvailable callback before we've created our image");

  mImage->GetStatusTracker().RecordDataAvailable(aCurrentFrame, rect);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendDataAvailable(iter.GetNext(), aCurrentFrame, rect);
  }

  return NS_OK;
}

/* void onStopFrame (in imgIRequest request, in unsigned long frame); */
NS_IMETHODIMP imgRequest::OnStopFrame(imgIRequest *request,
                                      PRUint32 frame)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopFrame");
  NS_ABORT_IF_FALSE(mImage,
                    "OnStopFrame callback before we've created our image");

  mImage->GetStatusTracker().RecordStopFrame(frame);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStopFrame(iter.GetNext(), frame);
  }

  return NS_OK;
}

/* void onStopContainer (in imgIRequest request, in imgIContainer image); */
NS_IMETHODIMP imgRequest::OnStopContainer(imgIRequest *request,
                                          imgIContainer *image)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopContainer");
  NS_ABORT_IF_FALSE(mImage,
                    "OnDataContainer callback before we've created our image");

  mImage->GetStatusTracker().RecordStopContainer(image);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStopContainer(iter.GetNext(), image);
  }

  return NS_OK;
}

/* void onStopDecode (in imgIRequest request, in nsresult status, in wstring statusArg); */
NS_IMETHODIMP imgRequest::OnStopDecode(imgIRequest *aRequest,
                                       nsresult aStatus,
                                       const PRUnichar *aStatusArg)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStopDecode");
  NS_ABORT_IF_FALSE(mImage,
                    "OnDataDecode callback before we've created our image");

  // We finished the decode, and thus have the decoded frames. Update the cache
  // entry size to take this into account.
  UpdateCacheEntrySize();

  mImage->GetStatusTracker().RecordStopDecode(aStatus, aStatusArg);

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendStopDecode(iter.GetNext(), aStatus,
                                              aStatusArg);
  }

  // RasterImage and everything below it is completely correct and
  // bulletproof about its handling of decoder notifications.
  // Unfortunately, here and above we have to make some gross and
  // inappropriate use of things to get things to work without
  // completely overhauling the decoder observer interface (this will,
  // thankfully, happen in bug 505385). From imgRequest and above (for
  // the time being), OnStopDecode is just a companion to OnStopRequest
  // that signals success or failure of the _load_ (not the _decode_).
  // Within imgStatusTracker, we ignore OnStopDecode notifications from the
  // decoder and RasterImage and generate our own every time we send
  // OnStopRequest. From within SendStopDecode, we actually send
  // OnStopContainer.  For more information, see bug 435296.

  return NS_OK;
}

NS_IMETHODIMP imgRequest::OnStopRequest(imgIRequest *aRequest,
                                        bool aLastPart)
{
  NS_NOTREACHED("imgRequest(imgIDecoderObserver)::OnStopRequest");
  return NS_OK;
}

/* void onDiscard (in imgIRequest request); */
NS_IMETHODIMP imgRequest::OnDiscard(imgIRequest *aRequest)
{
  NS_ABORT_IF_FALSE(mImage,
                    "OnDiscard callback before we've created our image");

  mImage->GetStatusTracker().RecordDiscard();

  // Update the cache entry size, since we just got rid of frame data
  UpdateCacheEntrySize();

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    mImage->GetStatusTracker().SendDiscard(iter.GetNext());
  }

  return NS_OK;
}

/** nsIRequestObserver methods **/

/* void onStartRequest (in nsIRequest request, in nsISupports ctxt); */
NS_IMETHODIMP imgRequest::OnStartRequest(nsIRequest *aRequest, nsISupports *ctxt)
{
  LOG_SCOPE(gImgLog, "imgRequest::OnStartRequest");

  // Figure out if we're multipart
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan)
      mIsMultiPartChannel = PR_TRUE;

  // If we're not multipart, we shouldn't have an image yet
  NS_ABORT_IF_FALSE(mIsMultiPartChannel || !mImage,
                    "Already have an image for non-multipart request");

  // If we're multipart, and our image is initialized, fix things up for another round
  if (mIsMultiPartChannel && mImage) {
    if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
      // Inform the RasterImage that we have new source data
      static_cast<RasterImage*>(mImage.get())->NewSourceData();
    } else {  // imageType == imgIContainer::TYPE_VECTOR
      nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
      NS_ABORT_IF_FALSE(imageAsStream,
                        "SVG-typed Image failed QI to nsIStreamListener");
      imageAsStream->OnStartRequest(aRequest, ctxt);
    }
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

  imgStatusTracker& statusTracker = GetStatusTracker();
  statusTracker.RecordStartRequest();

  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));
  if (channel)
    channel->GetSecurityInfo(getter_AddRefs(mSecurityInfo));

  nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
  while (iter.HasMore()) {
    statusTracker.SendStartRequest(iter.GetNext());
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

      // Tell all of our proxies that we have a principal.
      nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
      while (iter.HasMore()) {
        iter.GetNext()->SetPrincipal(mPrincipal);
      }
    }
  }

  SetCacheValidation(mCacheEntry, aRequest);

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

  bool lastPart = true;
  nsCOMPtr<nsIMultiPartChannel> mpchan(do_QueryInterface(aRequest));
  if (mpchan)
    mpchan->GetIsLastPart(&lastPart);

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
    nsresult rv;
    if (mImage->GetType() == imgIContainer::TYPE_RASTER) {
      // Notify the image
      rv = static_cast<RasterImage*>(mImage.get())->SourceDataComplete();
    } else { // imageType == imgIContainer::TYPE_VECTOR
      nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
      NS_ABORT_IF_FALSE(imageAsStream,
                        "SVG-typed Image failed QI to nsIStreamListener");
      rv = imageAsStream->OnStopRequest(aRequest, ctxt, status);
    }

    // If we got an error in the SourceDataComplete() / OnStopRequest() call,
    // we don't want to proceed as if nothing bad happened. However, we also
    // want to give precedence to failure status codes from necko, since
    // presumably they're more meaningful.
    if (NS_FAILED(rv) && NS_SUCCEEDED(status))
      status = rv;
  }

  imgStatusTracker& statusTracker = GetStatusTracker();
  statusTracker.RecordStopRequest(lastPart, status);

  // If the request went through, update the cache entry size. Otherwise,
  // cancel the request, which removes us from the cache.
  if (mImage && NS_SUCCEEDED(status)) {
    // We update the cache entry size here because this is where we finish
    // loading compressed source data, which is part of our size calculus.
    UpdateCacheEntrySize();
  }
  else {
    // stops animations, removes from cache
    this->Cancel(status);
  }

  /* notify the kids */
  nsTObserverArray<imgRequestProxy*>::ForwardIterator srIter(mObservers);
  while (srIter.HasMore()) {
    statusTracker.SendStopRequest(srIter.GetNext(), lastPart, status);
  }

  mTimedChannel = nsnull;
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

  nsresult rv;

  PRUint16 imageType;
  if (mGotData) {
    imageType = mImage->GetType();
  } else {
    LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |First time through... finding mimetype|");

    mGotData = PR_TRUE;

    /* look at the first few bytes and see if we can tell what the data is from that
     * since servers tend to lie. :(
     */
    PRUint32 out;
    inStr->ReadSegments(sniff_mimetype_callback, this, count, &out);

#ifdef NS_DEBUG
    /* NS_WARNING if the content type from the channel isn't the same if the sniffing */
#endif

    nsCOMPtr<nsIChannel> chan(do_QueryInterface(aRequest));
    if (mContentType.IsEmpty()) {
      LOG_SCOPE(gImgLog, "imgRequest::OnDataAvailable |sniffing of mimetype failed|");

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

    /* now we have mimetype, so we can infer the image type that we want */
    if (mContentType.EqualsLiteral(SVG_MIMETYPE)) {
      mImage = new VectorImage(mStatusTracker.forget());
    } else {
      mImage = new RasterImage(mStatusTracker.forget());
    }
    mImage->SetInnerWindowID(mInnerWindowId);
    imageType = mImage->GetType();

    // Notify any imgRequestProxys that are observing us that we have an Image.
    nsTObserverArray<imgRequestProxy*>::ForwardIterator iter(mObservers);
    while (iter.HasMore()) {
      iter.GetNext()->SetImage(mImage);
    }

    /* set our mimetype as a property */
    nsCOMPtr<nsISupportsCString> contentType(do_CreateInstance("@mozilla.org/supports-cstring;1"));
    if (contentType) {
      contentType->SetData(mContentType);
      mProperties->Set("type", contentType);
    }

    /* set our content disposition as a property */
    nsCAutoString disposition;
    if (chan) {
      chan->GetContentDispositionHeader(disposition);
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
    // Figure out our Image initialization flags
    //

    // We default to the static globals
    bool isDiscardable = gDiscardable;
    bool doDecodeOnDraw = gDecodeOnDraw;

    // We want UI to be as snappy as possible and not to flicker. Disable discarding
    // and decode-on-draw for chrome URLS
    bool isChrome = false;
    rv = mURI->SchemeIs("chrome", &isChrome);
    if (NS_SUCCEEDED(rv) && isChrome)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // We don't want resources like the "loading" icon to be discardable or
    // decode-on-draw either.
    bool isResource = false;
    rv = mURI->SchemeIs("resource", &isResource);
    if (NS_SUCCEEDED(rv) && isResource)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // For multipart/x-mixed-replace, we basically want a direct channel to the
    // decoder. Disable both for this case as well.
    if (mIsMultiPartChannel)
      isDiscardable = doDecodeOnDraw = PR_FALSE;

    // We have all the information we need
    PRUint32 imageFlags = Image::INIT_FLAG_NONE;
    if (isDiscardable)
      imageFlags |= Image::INIT_FLAG_DISCARDABLE;
    if (doDecodeOnDraw)
      imageFlags |= Image::INIT_FLAG_DECODE_ON_DRAW;
    if (mIsMultiPartChannel)
      imageFlags |= Image::INIT_FLAG_MULTIPART;

    // Get our URI string
    nsCAutoString uriString;
    rv = mURI->GetSpec(uriString);
    if (NS_FAILED(rv))
      uriString.Assign("<unknown image URI>");

    // Initialize the image that we created above. For RasterImages, this
    // instantiates a decoder behind the scenes, so if we don't have a decoder
    // for this mimetype we'll find out about it here.
    rv = mImage->Init(this, mContentType.get(), uriString.get(), imageFlags);
    if (NS_FAILED(rv)) { // Probably bad mimetype

      this->Cancel(rv);
      return NS_BINDING_ABORTED;
    }

    if (imageType == imgIContainer::TYPE_RASTER) {
      /* Use content-length as a size hint for http channels. */
      nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(aRequest));
      if (httpChannel) {
        nsCAutoString contentLength;
        rv = httpChannel->GetResponseHeader(NS_LITERAL_CSTRING("content-length"),
                                            contentLength);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 len = contentLength.ToInteger(&rv);

          // Pass anything usable on so that the RasterImage can preallocate
          // its source buffer
          if (len > 0) {
            PRUint32 sizeHint = (PRUint32) len;
            sizeHint = NS_MIN<PRUint32>(sizeHint, 20000000); /* Bound by something reasonable */
            RasterImage* rasterImage = static_cast<RasterImage*>(mImage.get());
            rv = rasterImage->SetSourceSizeHint(sizeHint);
            if (NS_FAILED(rv)) {
              // Flush memory, try to get some back, and try again
              rv = nsMemory::HeapMinimize(PR_TRUE);
              rv |= rasterImage->SetSourceSizeHint(sizeHint);
              // If we've still failed at this point, things are going downhill
              if (NS_FAILED(rv)) {
                NS_WARNING("About to hit OOM in imagelib!");
              }
            }
          }
        }
      }
    }

    if (imageType == imgIContainer::TYPE_RASTER) {
      // If we were waiting on the image to do something, now's our chance.
      if (mDecodeRequested) {
        mImage->RequestDecode();
      }
    } else { // imageType == imgIContainer::TYPE_VECTOR
      nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
      NS_ABORT_IF_FALSE(imageAsStream,
                        "SVG-typed Image failed QI to nsIStreamListener");
      imageAsStream->OnStartRequest(aRequest, nsnull);
    }
  }

  if (imageType == imgIContainer::TYPE_RASTER) {
    // WriteToRasterImage always consumes everything it gets
    // if it doesn't run out of memory
    PRUint32 bytesRead;
    rv = inStr->ReadSegments(RasterImage::WriteToRasterImage,
                             static_cast<void*>(mImage),
                             count, &bytesRead);
    NS_ABORT_IF_FALSE(bytesRead == count || mImage->HasError(),
  "WriteToRasterImage should consume everything or the image must be in error!");
  } else { // imageType == imgIContainer::TYPE_VECTOR
    nsCOMPtr<nsIStreamListener> imageAsStream = do_QueryInterface(mImage);
    rv = imageAsStream->OnDataAvailable(aRequest, ctxt, inStr,
                                        sourceOffset, count);
  }
  if (NS_FAILED(rv)) {
    PR_LOG(gImgLog, PR_LOG_WARNING,
           ("[this=%p] imgRequest::OnDataAvailable -- "
            "copy to RasterImage failed\n", this));
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
NS_IMETHODIMP
imgRequest::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                   nsIChannel *newChannel, PRUint32 flags,
                                   nsIAsyncVerifyRedirectCallback *callback)
{
  NS_ASSERTION(mRequest && mChannel, "Got a channel redirect after we nulled out mRequest!");
  NS_ASSERTION(mChannel == oldChannel, "Got a channel redirect for an unknown channel!");
  NS_ASSERTION(newChannel, "Got a redirect to a NULL channel!");

  SetCacheValidation(mCacheEntry, oldChannel);

  // Prepare for callback
  mRedirectCallback = callback;
  mNewRedirectChannel = newChannel;

  nsCOMPtr<nsIChannelEventSink> sink(do_GetInterface(mPrevChannelSink));
  if (sink) {
    nsresult rv = sink->AsyncOnChannelRedirect(oldChannel, newChannel, flags,
                                               this);
    if (NS_FAILED(rv)) {
        mRedirectCallback = nsnull;
        mNewRedirectChannel = nsnull;
    }
    return rv;
  }

  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
imgRequest::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  if (NS_FAILED(result)) {
      mRedirectCallback->OnRedirectVerifyCallback(result);
      mRedirectCallback = nsnull;
      mNewRedirectChannel = nsnull;
      return NS_OK;
  }

  mChannel = mNewRedirectChannel;
  mTimedChannel = do_QueryInterface(mChannel);
  mNewRedirectChannel = nsnull;

#if defined(PR_LOGGING)
  nsCAutoString oldspec;
  if (mCurrentURI)
    mCurrentURI->GetSpec(oldspec);
  LOG_MSG_WITH_PARAM(gImgLog, "imgRequest::OnChannelRedirect", "old", oldspec.get());
#endif

  // make sure we have a protocol that returns data rather than opens
  // an external application, e.g. mailto:
  mChannel->GetURI(getter_AddRefs(mCurrentURI));
  bool doesNotReturnData = false;
  nsresult rv =
    NS_URIChainHasFlags(mCurrentURI, nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                        &doesNotReturnData);

  if (NS_SUCCEEDED(rv) && doesNotReturnData)
    rv = NS_ERROR_ABORT;

  if (NS_FAILED(rv)) {
    mRedirectCallback->OnRedirectVerifyCallback(rv);
    mRedirectCallback = nsnull;
    return NS_OK;
  }

  mRedirectCallback->OnRedirectVerifyCallback(NS_OK);
  mRedirectCallback = nsnull;
  return NS_OK;
}
