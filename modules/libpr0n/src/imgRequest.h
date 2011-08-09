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

#ifndef imgRequest_h__
#define imgRequest_h__

#include "imgIDecoderObserver.h"

#include "nsIChannelEventSink.h"
#include "nsIContentSniffer.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRequest.h"
#include "nsIProperties.h"
#include "nsIStreamListener.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsITimedChannel.h"

#include "nsCategoryCache.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTObserverArray.h"
#include "nsWeakReference.h"
#include "ImageErrors.h"
#include "imgIRequest.h"
#include "imgStatusTracker.h"
#include "nsIAsyncVerifyRedirectCallback.h"

class imgCacheValidator;

class imgRequestProxy;
class imgCacheEntry;
class imgMemoryReporter;
class imgRequestNotifyRunnable;

namespace mozilla {
namespace imagelib {
class Image;
} // namespace imagelib
} // namespace mozilla

class imgRequest : public imgIDecoderObserver,
                   public nsIStreamListener,
                   public nsSupportsWeakReference,
                   public nsIChannelEventSink,
                   public nsIInterfaceRequestor,
                   public nsIAsyncVerifyRedirectCallback
{
public:
  imgRequest();
  virtual ~imgRequest();

  NS_DECL_ISUPPORTS

  nsresult Init(nsIURI *aURI,
                nsIURI *aCurrentURI,
                nsIRequest *aRequest,
                nsIChannel *aChannel,
                imgCacheEntry *aCacheEntry,
                void *aCacheId,
                void *aLoadId,
                nsIPrincipal* aLoadingPrincipal,
                PRInt32 aCORSMode);

  // Callers must call imgRequestProxy::Notify later.
  nsresult AddProxy(imgRequestProxy *proxy);

  // aNotify==PR_FALSE still sends OnStopRequest.
  nsresult RemoveProxy(imgRequestProxy *proxy, nsresult aStatus, PRBool aNotify);

  void SniffMimeType(const char *buf, PRUint32 len);

  // a request is "reusable" if it has already been loaded, or it is
  // currently being loaded on the same event queue as the new request
  // being made...
  PRBool IsReusable(void *aCacheId);

  // Cancel, but also ensure that all work done in Init() is undone. Call this
  // only when the channel has failed to open, and so calling Cancel() on it
  // won't be sufficient.
  void CancelAndAbort(nsresult aStatus);

  // Methods that get forwarded to the Image, or deferred until it's
  // instantiated.
  nsresult LockImage();
  nsresult UnlockImage();
  nsresult RequestDecode();

  inline void SetWindowID(PRUint64 aWindowId) {
    mWindowId = aWindowId;
  }

  inline PRUint64 WindowID() const {
    return mWindowId;
  }

  // Set the cache validation information (expiry time, whether we must
  // validate, etc) on the cache entry based on the request information.
  // If this function is called multiple times, the information set earliest
  // wins.
  static void SetCacheValidation(imgCacheEntry* aEntry, nsIRequest* aRequest);

  // The CORS mode for which we loaded this image.
  PRInt32 GetCORSMode() const { return mCORSMode; }

  // The principal for the document that loaded this image. Used when trying to
  // validate a CORS image load.
  already_AddRefed<nsIPrincipal> GetLoadingPrincipal() const
  {
    nsCOMPtr<nsIPrincipal> principal = mLoadingPrincipal;
    return principal.forget();
  }

private:
  friend class imgCacheEntry;
  friend class imgRequestProxy;
  friend class imgLoader;
  friend class imgCacheValidator;
  friend class imgStatusTracker;
  friend class imgCacheExpirationTracker;
  friend class imgRequestNotifyRunnable;

  inline void SetLoadId(void *aLoadId) {
    mLoadId = aLoadId;
  }
  void Cancel(nsresult aStatus);
  void RemoveFromCache();

  nsresult GetURI(nsIURI **aURI);
  nsresult GetKeyURI(nsIURI **aURI);
  nsresult GetSecurityInfo(nsISupports **aSecurityInfo);

  inline const char *GetMimeType() const {
    return mContentType.get();
  }
  inline nsIProperties *Properties() {
    return mProperties;
  }

  // Return the imgStatusTracker associated with this imgRequest.  It may live
  // in |mStatusTracker| or in |mImage.mStatusTracker|, depending on whether
  // mImage has been instantiated yet..
  imgStatusTracker& GetStatusTracker();
    
  // Reset the cache entry after we've dropped our reference to it. Used by the
  // imgLoader when our cache entry is re-requested after we've dropped our
  // reference to it.
  void SetCacheEntry(imgCacheEntry *entry);

  // Returns whether we've got a reference to the cache entry.
  PRBool HasCacheEntry() const;

  // Return true if at least one of our proxies, excluding
  // aProxyToIgnore, has an observer.  aProxyToIgnore may be null.
  PRBool HaveProxyWithObserver(imgRequestProxy* aProxyToIgnore) const;

  // Return the priority of the underlying network request, or return
  // PRIORITY_NORMAL if it doesn't support nsISupportsPriority.
  PRInt32 Priority() const;

  // Adjust the priority of the underlying network request by the given delta
  // on behalf of the given proxy.
  void AdjustPriority(imgRequestProxy *aProxy, PRInt32 aDelta);

  // Return whether we've seen some data at this point
  PRBool HasTransferredData() const { return mGotData; }

  // Set whether this request is stored in the cache. If it isn't, regardless
  // of whether this request has a non-null mCacheEntry, this imgRequest won't
  // try to update or modify the image cache.
  void SetIsInCache(PRBool cacheable);

  // Update the cache entry size based on the image container
  void UpdateCacheEntrySize();

public:
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

private:
  friend class imgMemoryReporter;

  nsCOMPtr<nsIRequest> mRequest;
  // The original URI we were loaded with. This is the same as the URI we are
  // keyed on in the cache.
  nsCOMPtr<nsIURI> mURI;
  // The URI of the resource we ended up loading after all redirects, etc.
  nsCOMPtr<nsIURI> mCurrentURI;
  // The principal of the document which loaded this image. Used when validating for CORS.
  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  // The principal of this image.
  nsCOMPtr<nsIPrincipal> mPrincipal;
  // Status-tracker -- transferred to mImage, when it gets instantiated
  nsAutoPtr<imgStatusTracker> mStatusTracker;
  nsRefPtr<mozilla::imagelib::Image> mImage;
  nsCOMPtr<nsIProperties> mProperties;
  nsCOMPtr<nsISupports> mSecurityInfo;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIInterfaceRequestor> mPrevChannelSink;

  nsTObserverArray<imgRequestProxy*> mObservers;

  nsCOMPtr<nsITimedChannel> mTimedChannel;

  nsCString mContentType;

  nsRefPtr<imgCacheEntry> mCacheEntry; /* we hold on to this to this so long as we have observers */

  void *mCacheId;

  void *mLoadId;

  imgCacheValidator *mValidator;
  nsCategoryCache<nsIContentSniffer> mImageSniffers;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mNewRedirectChannel;

  // Originating outer window ID. Used for error reporting.
  PRUint64 mWindowId;

  // The CORS mode (defined in imgIRequest) this image was loaded with. By
  // default, imgIRequest::CORS_NONE.
  PRInt32 mCORSMode;

  // Sometimes consumers want to do things before the image is ready. Let them,
  // and apply the action when the image becomes available.
  PRPackedBool mDecodeRequested : 1;

  PRPackedBool mIsMultiPartChannel : 1;
  PRPackedBool mGotData : 1;
  PRPackedBool mIsInCache : 1;
};

#endif
