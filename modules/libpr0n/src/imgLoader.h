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
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
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

#ifndef imgLoader_h__
#define imgLoader_h__

#include "imgILoader.h"
#include "imgICache.h"
#include "nsWeakReference.h"
#include "nsIContentSniffer.h"
#include "nsRefPtrHashtable.h"
#include "nsExpirationTracker.h"
#include "nsAutoPtr.h"
#include "prtypes.h"
#include "imgRequest.h"
#include "nsIObserverService.h"
#include "nsIChannelPolicy.h"
#include "nsIProgressEventSink.h"
#include "nsIChannel.h"

#ifdef LOADER_THREADSAFE
#include "prlock.h"
#endif

class imgRequest;
class imgRequestProxy;
class imgIRequest;
class imgIDecoderObserver;
class nsILoadGroup;

class imgCacheEntry
{
public:
  imgCacheEntry(imgRequest *request, bool aForcePrincipalCheck);
  ~imgCacheEntry();

  nsrefcnt AddRef()
  {
    NS_PRECONDITION(PRInt32(mRefCnt) >= 0, "illegal refcnt");
    NS_ABORT_IF_FALSE(_mOwningThread.GetThread() == PR_GetCurrentThread(), "imgCacheEntry addref isn't thread-safe!");
    ++mRefCnt;
    NS_LOG_ADDREF(this, mRefCnt, "imgCacheEntry", sizeof(*this));
    return mRefCnt;
  }
 
  nsrefcnt Release()
  {
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    NS_ABORT_IF_FALSE(_mOwningThread.GetThread() == PR_GetCurrentThread(), "imgCacheEntry release isn't thread-safe!");
    --mRefCnt;
    NS_LOG_RELEASE(this, mRefCnt, "imgCacheEntry");
    if (mRefCnt == 0) {
      mRefCnt = 1; /* stabilize */
      delete this;
      return 0;
    }
    return mRefCnt;                              
  }

  PRUint32 GetDataSize() const
  {
    return mDataSize;
  }
  void SetDataSize(PRUint32 aDataSize)
  {
    PRInt32 oldsize = mDataSize;
    mDataSize = aDataSize;
    UpdateCache(mDataSize - oldsize);
  }

  PRInt32 GetTouchedTime() const
  {
    return mTouchedTime;
  }
  void SetTouchedTime(PRInt32 time)
  {
    mTouchedTime = time;
    Touch(/* updateTime = */ PR_FALSE);
  }

  PRInt32 GetExpiryTime() const
  {
    return mExpiryTime;
  }
  void SetExpiryTime(PRInt32 aExpiryTime)
  {
    mExpiryTime = aExpiryTime;
    Touch();
  }

  bool GetMustValidate() const
  {
    return mMustValidate;
  }
  void SetMustValidate(bool aValidate)
  {
    mMustValidate = aValidate;
    Touch();
  }

  already_AddRefed<imgRequest> GetRequest() const
  {
    imgRequest *req = mRequest;
    NS_ADDREF(req);
    return req;
  }

  bool Evicted() const
  {
    return mEvicted;
  }

  nsExpirationState *GetExpirationState()
  {
    return &mExpirationState;
  }

  bool HasNoProxies() const
  {
    return mHasNoProxies;
  }

  bool ForcePrincipalCheck() const
  {
    return mForcePrincipalCheck;
  }

private: // methods
  friend class imgLoader;
  friend class imgCacheQueue;
  void Touch(bool updateTime = true);
  void UpdateCache(PRInt32 diff = 0);
  void SetEvicted(bool evict)
  {
    mEvicted = evict;
  }
  void SetHasNoProxies(bool hasNoProxies);

  // Private, unimplemented copy constructor.
  imgCacheEntry(const imgCacheEntry &);

private: // data
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD

  nsRefPtr<imgRequest> mRequest;
  PRUint32 mDataSize;
  PRInt32 mTouchedTime;
  PRInt32 mExpiryTime;
  nsExpirationState mExpirationState;
  bool mMustValidate : 1;
  bool mEvicted : 1;
  bool mHasNoProxies : 1;
  bool mForcePrincipalCheck : 1;
};

#include <vector>

#define NS_IMGLOADER_CID \
{ /* 9f6a0d2e-1dd1-11b2-a5b8-951f13c846f7 */         \
     0x9f6a0d2e,                                     \
     0x1dd1,                                         \
     0x11b2,                                         \
    {0xa5, 0xb8, 0x95, 0x1f, 0x13, 0xc8, 0x46, 0xf7} \
}

class imgCacheQueue
{
public: 
  imgCacheQueue();
  void Remove(imgCacheEntry *);
  void Push(imgCacheEntry *);
  void MarkDirty();
  bool IsDirty();
  already_AddRefed<imgCacheEntry> Pop();
  void Refresh();
  PRUint32 GetSize() const;
  void UpdateSize(PRInt32 diff);
  PRUint32 GetNumElements() const;
  typedef std::vector<nsRefPtr<imgCacheEntry> > queueContainer;  
  typedef queueContainer::iterator iterator;
  typedef queueContainer::const_iterator const_iterator;

  iterator begin();
  const_iterator begin() const;
  iterator end();
  const_iterator end() const;

private:
  queueContainer mQueue;
  bool mDirty;
  PRUint32 mSize;
};

class imgMemoryReporter;

class imgLoader : public imgILoader,
                  public nsIContentSniffer,
                  public imgICache,
                  public nsSupportsWeakReference,
                  public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGILOADER
  NS_DECL_NSICONTENTSNIFFER
  NS_DECL_IMGICACHE
  NS_DECL_NSIOBSERVER

  imgLoader();
  virtual ~imgLoader();

  nsresult Init();

  static nsresult GetMimeTypeFromContent(const char* aContents, PRUint32 aLength, nsACString& aContentType);

  static void Shutdown(); // for use by the factory

  static nsresult ClearChromeImageCache();
  static nsresult ClearImageCache();
  static void MinimizeCaches();

  static nsresult InitCache();

  static bool RemoveFromCache(nsIURI *aKey);
  static bool RemoveFromCache(imgCacheEntry *entry);

  static bool PutIntoCache(nsIURI *key, imgCacheEntry *entry);

  // Returns true if we should prefer evicting cache entry |two| over cache
  // entry |one|.
  // This mixes units in the worst way, but provides reasonable results.
  inline static bool CompareCacheEntries(const nsRefPtr<imgCacheEntry> &one,
                                         const nsRefPtr<imgCacheEntry> &two)
  {
    if (!one)
      return false;
    if (!two)
      return true;

    const double sizeweight = 1.0 - sCacheTimeWeight;

    // We want large, old images to be evicted first (depending on their
    // relative weights). Since a larger time is actually newer, we subtract
    // time's weight, so an older image has a larger weight.
    double oneweight = double(one->GetDataSize()) * sizeweight -
                       double(one->GetTouchedTime()) * sCacheTimeWeight;
    double twoweight = double(two->GetDataSize()) * sizeweight -
                       double(two->GetTouchedTime()) * sCacheTimeWeight;

    return oneweight < twoweight;
  }

  static void VerifyCacheSizes();

  // The image loader maintains a hash table of all imgCacheEntries. However,
  // only some of them will be evicted from the cache: those who have no
  // imgRequestProxies watching their imgRequests. 
  //
  // Once an imgRequest has no imgRequestProxies, it should notify us by
  // calling HasNoObservers(), and null out its cache entry pointer.
  // 
  // Upon having a proxy start observing again, it should notify us by calling
  // HasObservers(). The request's cache entry will be re-set before this
  // happens, by calling imgRequest::SetCacheEntry() when an entry with no
  // observers is re-requested.
  static bool SetHasNoProxies(nsIURI *key, imgCacheEntry *entry);
  static bool SetHasProxies(nsIURI *key);

private: // methods


  bool ValidateEntry(imgCacheEntry *aEntry, nsIURI *aKey,
                       nsIURI *aInitialDocumentURI, nsIURI *aReferrerURI, 
                       nsILoadGroup *aLoadGroup,
                       imgIDecoderObserver *aObserver, nsISupports *aCX,
                       nsLoadFlags aLoadFlags, bool aCanMakeNewChannel,
                       imgIRequest *aExistingRequest,
                       imgIRequest **aProxyRequest,
                       nsIChannelPolicy *aPolicy,
                       nsIPrincipal* aLoadingPrincipal,
                       PRInt32 aCORSMode);
  bool ValidateRequestWithNewChannel(imgRequest *request, nsIURI *aURI,
                                       nsIURI *aInitialDocumentURI,
                                       nsIURI *aReferrerURI,
                                       nsILoadGroup *aLoadGroup,
                                       imgIDecoderObserver *aObserver,
                                       nsISupports *aCX, nsLoadFlags aLoadFlags,
                                       imgIRequest *aExistingRequest,
                                       imgIRequest **aProxyRequest,
                                       nsIChannelPolicy *aPolicy,
                                       nsIPrincipal* aLoadingPrincipal,
                                       PRInt32 aCORSMode);

  nsresult CreateNewProxyForRequest(imgRequest *aRequest, nsILoadGroup *aLoadGroup,
                                    imgIDecoderObserver *aObserver,
                                    nsLoadFlags aLoadFlags, imgIRequest *aRequestProxy,
                                    imgIRequest **_retval);

  void ReadAcceptHeaderPref();


  typedef nsRefPtrHashtable<nsCStringHashKey, imgCacheEntry> imgCacheTable;

  static nsresult EvictEntries(imgCacheTable &aCacheToClear);
  static nsresult EvictEntries(imgCacheQueue &aQueueToClear);

  static imgCacheTable &GetCache(nsIURI *aURI);
  static imgCacheQueue &GetCacheQueue(nsIURI *aURI);
  static void CacheEntriesChanged(nsIURI *aURI, PRInt32 sizediff = 0);
  static void CheckCacheLimits(imgCacheTable &cache, imgCacheQueue &queue);

private: // data
  friend class imgCacheEntry;
  friend class imgMemoryReporter;

  static imgCacheTable sCache;
  static imgCacheQueue sCacheQueue;

  static imgCacheTable sChromeCache;
  static imgCacheQueue sChromeCacheQueue;
  static PRFloat64 sCacheTimeWeight;
  static PRUint32 sCacheMaxSize;

  nsCString mAcceptHeader;
};



/**
 * proxy stream listener class used to handle multipart/x-mixed-replace
 */

#include "nsCOMPtr.h"
#include "nsIStreamListener.h"

class ProxyListener : public nsIStreamListener
{
public:
  ProxyListener(nsIStreamListener *dest);
  virtual ~ProxyListener();

  /* additional members */
  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

private:
  nsCOMPtr<nsIStreamListener> mDestListener;
};

/**
 * A class that implements nsIProgressEventSink and forwards all calls to it to
 * the original notification callbacks of the channel. Also implements
 * nsIInterfaceRequestor and gives out itself for nsIProgressEventSink calls,
 * and forwards everything else to the channel's notification callbacks.
 */
class nsProgressNotificationProxy : public nsIProgressEventSink
                                  , public nsIChannelEventSink
                                  , public nsIInterfaceRequestor
{
  public:
    nsProgressNotificationProxy(nsIChannel* channel,
                                imgIRequest* proxy)
        : mImageRequest(proxy) {
      channel->GetNotificationCallbacks(getter_AddRefs(mOriginalCallbacks));
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROGRESSEVENTSINK
    NS_DECL_NSICHANNELEVENTSINK
    NS_DECL_NSIINTERFACEREQUESTOR
  private:
    ~nsProgressNotificationProxy() {}

    nsCOMPtr<nsIInterfaceRequestor> mOriginalCallbacks;
    nsCOMPtr<nsIRequest> mImageRequest;
};

/**
 * validate checker
 */

#include "nsCOMArray.h"

class imgCacheValidator : public nsIStreamListener,
                          public nsIChannelEventSink,
                          public nsIInterfaceRequestor,
                          public nsIAsyncVerifyRedirectCallback
{
public:
  imgCacheValidator(nsProgressNotificationProxy* progress, imgRequest *request,
                    void *aContext, bool forcePrincipalCheckForCacheEntry);
  virtual ~imgCacheValidator();

  void AddProxy(imgRequestProxy *aProxy);

  NS_DECL_ISUPPORTS
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

private:
  nsCOMPtr<nsIStreamListener> mDestListener;
  nsRefPtr<nsProgressNotificationProxy> mProgressProxy;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel> mRedirectChannel;

  nsRefPtr<imgRequest> mRequest;
  nsCOMArray<imgIRequest> mProxies;

  nsRefPtr<imgRequest> mNewRequest;
  nsRefPtr<imgCacheEntry> mNewEntry;

  void *mContext;

  static imgLoader sImgLoader;
};

#endif  // imgLoader_h__
