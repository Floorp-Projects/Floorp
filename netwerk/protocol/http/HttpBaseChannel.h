/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_HttpBaseChannel_h
#define mozilla_net_HttpBaseChannel_h

#include "nsHttp.h"
#include "nsAutoPtr.h"
#include "nsHashPropertyBag.h"
#include "nsProxyInfo.h"
#include "nsHttpRequestHead.h"
#include "nsHttpResponseHead.h"
#include "nsHttpConnectionInfo.h"
#include "nsIEncodedChannel.h"
#include "nsIHttpChannel.h"
#include "nsHttpHandler.h"
#include "nsIHttpChannelInternal.h"
#include "nsIForcePendingChannel.h"
#include "nsIRedirectHistory.h"
#include "nsIUploadChannel.h"
#include "nsIUploadChannel2.h"
#include "nsIProgressEventSink.h"
#include "nsIURI.h"
#include "nsIEffectiveTLDService.h"
#include "nsIStringEnumerator.h"
#include "nsISupportsPriority.h"
#include "nsIApplicationCache.h"
#include "nsIResumableChannel.h"
#include "nsITraceableChannel.h"
#include "nsILoadContext.h"
#include "nsILoadInfo.h"
#include "mozilla/net/NeckoCommon.h"
#include "nsThreadUtils.h"
#include "PrivateBrowsingChannel.h"
#include "mozilla/net/DNS.h"
#include "nsITimedChannel.h"
#include "nsISecurityConsoleMessage.h"

extern PRLogModuleInfo *gHttpLog;

namespace mozilla {
namespace net {

/*
 * This class is a partial implementation of nsIHttpChannel.  It contains code
 * shared by nsHttpChannel and HttpChannelChild.
 * - Note that this class has nothing to do with nsBaseChannel, which is an
 *   earlier effort at a base class for channels that somehow never made it all
 *   the way to the HTTP channel.
 */
class HttpBaseChannel : public nsHashPropertyBag
                      , public nsIEncodedChannel
                      , public nsIHttpChannel
                      , public nsIHttpChannelInternal
                      , public nsIRedirectHistory
                      , public nsIUploadChannel
                      , public nsIUploadChannel2
                      , public nsISupportsPriority
                      , public nsIResumableChannel
                      , public nsITraceableChannel
                      , public PrivateBrowsingChannel<HttpBaseChannel>
                      , public nsITimedChannel
                      , public nsIForcePendingChannel
{
protected:
  virtual ~HttpBaseChannel();

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIUPLOADCHANNEL
  NS_DECL_NSIUPLOADCHANNEL2
  NS_DECL_NSITRACEABLECHANNEL
  NS_DECL_NSITIMEDCHANNEL
  NS_DECL_NSIREDIRECTHISTORY

  HttpBaseChannel();

  virtual nsresult Init(nsIURI *aURI, uint32_t aCaps, nsProxyInfo *aProxyInfo,
                        uint32_t aProxyResolveFlags,
                        nsIURI *aProxyURI);

  // nsIRequest
  NS_IMETHOD GetName(nsACString& aName);
  NS_IMETHOD IsPending(bool *aIsPending);
  NS_IMETHOD GetStatus(nsresult *aStatus);
  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup);
  NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup);
  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags);
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags);

  // nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI);
  NS_IMETHOD SetOriginalURI(nsIURI *aOriginalURI);
  NS_IMETHOD GetURI(nsIURI **aURI);
  NS_IMETHOD GetOwner(nsISupports **aOwner);
  NS_IMETHOD SetOwner(nsISupports *aOwner);
  NS_IMETHOD GetLoadInfo(nsILoadInfo **aLoadInfo);
  NS_IMETHOD SetLoadInfo(nsILoadInfo *aLoadInfo);
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks);
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks);
  NS_IMETHOD GetContentType(nsACString& aContentType);
  NS_IMETHOD SetContentType(const nsACString& aContentType);
  NS_IMETHOD GetContentCharset(nsACString& aContentCharset);
  NS_IMETHOD SetContentCharset(const nsACString& aContentCharset);
  NS_IMETHOD GetContentDisposition(uint32_t *aContentDisposition);
  NS_IMETHOD SetContentDisposition(uint32_t aContentDisposition);
  NS_IMETHOD GetContentDispositionFilename(nsAString& aContentDispositionFilename);
  NS_IMETHOD SetContentDispositionFilename(const nsAString& aContentDispositionFilename);
  NS_IMETHOD GetContentDispositionHeader(nsACString& aContentDispositionHeader);
  NS_IMETHOD GetContentLength(int64_t *aContentLength);
  NS_IMETHOD SetContentLength(int64_t aContentLength);
  NS_IMETHOD Open(nsIInputStream **aResult);

  // nsIEncodedChannel
  NS_IMETHOD GetApplyConversion(bool *value);
  NS_IMETHOD SetApplyConversion(bool value);
  NS_IMETHOD GetContentEncodings(nsIUTF8StringEnumerator** aEncodings);

  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD GetRequestMethod(nsACString& aMethod);
  NS_IMETHOD SetRequestMethod(const nsACString& aMethod);
  NS_IMETHOD GetReferrer(nsIURI **referrer);
  NS_IMETHOD SetReferrer(nsIURI *referrer);
  NS_IMETHOD GetRequestHeader(const nsACString& aHeader, nsACString& aValue);
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue, bool aMerge);
  NS_IMETHOD VisitRequestHeaders(nsIHttpHeaderVisitor *visitor);
  NS_IMETHOD GetResponseHeader(const nsACString &header, nsACString &value);
  NS_IMETHOD SetResponseHeader(const nsACString& header,
                               const nsACString& value, bool merge);
  NS_IMETHOD VisitResponseHeaders(nsIHttpHeaderVisitor *visitor);
  NS_IMETHOD GetAllowPipelining(bool *value);
  NS_IMETHOD SetAllowPipelining(bool value);
  NS_IMETHOD GetAllowSTS(bool *value);
  NS_IMETHOD SetAllowSTS(bool value);
  NS_IMETHOD GetRedirectionLimit(uint32_t *value);
  NS_IMETHOD SetRedirectionLimit(uint32_t value);
  NS_IMETHOD IsNoStoreResponse(bool *value);
  NS_IMETHOD IsNoCacheResponse(bool *value);
  NS_IMETHOD GetResponseStatus(uint32_t *aValue);
  NS_IMETHOD GetResponseStatusText(nsACString& aValue);
  NS_IMETHOD GetRequestSucceeded(bool *aValue);
  NS_IMETHOD RedirectTo(nsIURI *newURI);

  // nsIHttpChannelInternal
  NS_IMETHOD GetDocumentURI(nsIURI **aDocumentURI);
  NS_IMETHOD SetDocumentURI(nsIURI *aDocumentURI);
  NS_IMETHOD GetRequestVersion(uint32_t *major, uint32_t *minor);
  NS_IMETHOD GetResponseVersion(uint32_t *major, uint32_t *minor);
  NS_IMETHOD SetCookie(const char *aCookieHeader);
  NS_IMETHOD GetForceAllowThirdPartyCookie(bool *aForce);
  NS_IMETHOD SetForceAllowThirdPartyCookie(bool aForce);
  NS_IMETHOD GetCanceled(bool *aCanceled);
  NS_IMETHOD GetChannelIsForDownload(bool *aChannelIsForDownload);
  NS_IMETHOD SetChannelIsForDownload(bool aChannelIsForDownload);
  NS_IMETHOD SetCacheKeysRedirectChain(nsTArray<nsCString> *cacheKeys);
  NS_IMETHOD GetLocalAddress(nsACString& addr);
  NS_IMETHOD GetLocalPort(int32_t* port);
  NS_IMETHOD GetRemoteAddress(nsACString& addr);
  NS_IMETHOD GetRemotePort(int32_t* port);
  NS_IMETHOD GetAllowSpdy(bool *aAllowSpdy);
  NS_IMETHOD SetAllowSpdy(bool aAllowSpdy);
  NS_IMETHOD GetLoadAsBlocking(bool *aLoadAsBlocking);
  NS_IMETHOD SetLoadAsBlocking(bool aLoadAsBlocking);
  NS_IMETHOD GetLoadUnblocked(bool *aLoadUnblocked);
  NS_IMETHOD SetLoadUnblocked(bool aLoadUnblocked);
  NS_IMETHOD GetApiRedirectToURI(nsIURI * *aApiRedirectToURI);
  NS_IMETHOD AddSecurityMessage(const nsAString &aMessageTag, const nsAString &aMessageCategory);
  NS_IMETHOD TakeAllSecurityMessages(nsCOMArray<nsISecurityConsoleMessage> &aMessages);
  NS_IMETHOD GetResponseTimeoutEnabled(bool *aEnable);
  NS_IMETHOD SetResponseTimeoutEnabled(bool aEnable);
  NS_IMETHOD AddRedirect(nsIPrincipal *aRedirect);
  NS_IMETHOD ForcePending(bool aForcePending);
  NS_IMETHOD GetLastModifiedTime(PRTime* lastModifiedTime);

  inline void CleanRedirectCacheChainIfNecessary()
  {
      mRedirectedCachekeys = nullptr;
  }
  NS_IMETHOD HTTPUpgrade(const nsACString & aProtocolName,
                         nsIHttpUpgradeListener *aListener);

  // nsISupportsPriority
  NS_IMETHOD GetPriority(int32_t *value);
  NS_IMETHOD AdjustPriority(int32_t delta);

  // nsIResumableChannel
  NS_IMETHOD GetEntityID(nsACString& aEntityID);

  class nsContentEncodings : public nsIUTF8StringEnumerator
    {
    public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSIUTF8STRINGENUMERATOR

        nsContentEncodings(nsIHttpChannel* aChannel, const char* aEncodingHeader);

    private:
        virtual ~nsContentEncodings();

        nsresult PrepareForNext(void);

        // We do not own the buffer.  The channel owns it.
        const char* mEncodingHeader;
        const char* mCurStart;  // points to start of current header
        const char* mCurEnd;  // points to end of current header

        // Hold a ref to our channel so that it can't go away and take the
        // header with it.
        nsCOMPtr<nsIHttpChannel> mChannel;

        bool mReady;
    };

    nsHttpResponseHead * GetResponseHead() const { return mResponseHead; }
    nsHttpRequestHead * GetRequestHead() { return &mRequestHead; }

    const NetAddr& GetSelfAddr() { return mSelfAddr; }
    const NetAddr& GetPeerAddr() { return mPeerAddr; }

public: /* Necko internal use only... */


    // Return whether upon a redirect code of httpStatus for method, the
    // request method should be rewritten to GET.
    static bool ShouldRewriteRedirectToGET(uint32_t httpStatus,
                                           nsHttpRequestHead::ParsedMethodType method);

protected:
  nsCOMArray<nsISecurityConsoleMessage> mSecurityConsoleMessages;

  // Handle notifying listener, removing from loadgroup if request failed.
  void     DoNotifyListener();
  virtual void DoNotifyListenerCleanup() = 0;

  // drop reference to listener, its callbacks, and the progress sink
  void ReleaseListeners();

  nsresult ApplyContentConversions();

  void AddCookiesToRequest();
  virtual nsresult SetupReplacementChannel(nsIURI *,
                                           nsIChannel *,
                                           bool preserveMethod);

  // bundle calling OMR observers and marking flag into one function
  inline void CallOnModifyRequestObservers() {
    gHttpHandler->OnModifyRequest(this);
    mRequestObserversCalled = true;
  }

  // Helper function to simplify getting notification callbacks.
  template <class T>
  void GetCallback(nsCOMPtr<T> &aResult)
  {
    NS_QueryNotificationCallbacks(mCallbacks, mLoadGroup,
                                  NS_GET_TEMPLATE_IID(T),
                                  getter_AddRefs(aResult));
  }

  // Redirect tracking
  // Checks whether or not aURI and mOriginalURI share the same domain.
  bool SameOriginWithOriginalUri(nsIURI *aURI);

  // GetPrincipal
  // Returns the channel principal. If requireAppId is true, then returns
  // null if the principal has unknown appId.
  nsIPrincipal *GetPrincipal(bool requireAppId);

  friend class PrivateBrowsingChannel<HttpBaseChannel>;

  nsCOMPtr<nsIURI>                  mURI;
  nsCOMPtr<nsIURI>                  mOriginalURI;
  nsCOMPtr<nsIURI>                  mDocumentURI;
  nsCOMPtr<nsIStreamListener>       mListener;
  nsCOMPtr<nsISupports>             mListenerContext;
  nsCOMPtr<nsILoadGroup>            mLoadGroup;
  nsCOMPtr<nsISupports>             mOwner;
  nsCOMPtr<nsILoadInfo>             mLoadInfo;
  nsCOMPtr<nsIInterfaceRequestor>   mCallbacks;
  nsCOMPtr<nsIProgressEventSink>    mProgressSink;
  nsCOMPtr<nsIURI>                  mReferrer;
  nsCOMPtr<nsIApplicationCache>     mApplicationCache;

  nsHttpRequestHead                 mRequestHead;
  nsCOMPtr<nsIInputStream>          mUploadStream;
  nsAutoPtr<nsHttpResponseHead>     mResponseHead;
  nsRefPtr<nsHttpConnectionInfo>    mConnectionInfo;
  nsCOMPtr<nsIProxyInfo>            mProxyInfo;

  nsCString                         mSpec; // ASCII encoded URL spec
  nsCString                         mContentTypeHint;
  nsCString                         mContentCharsetHint;
  nsCString                         mUserSetCookieHeader;

  NetAddr                           mSelfAddr;
  NetAddr                           mPeerAddr;

  // HTTP Upgrade Data
  nsCString                        mUpgradeProtocol;
  nsCOMPtr<nsIHttpUpgradeListener> mUpgradeProtocolCallback;

  // Resumable channel specific data
  nsCString                         mEntityID;
  uint64_t                          mStartPos;

  nsresult                          mStatus;
  uint32_t                          mLoadFlags;
  uint32_t                          mCaps;
  int16_t                           mPriority;
  uint8_t                           mRedirectionLimit;

  uint32_t                          mApplyConversion            : 1;
  uint32_t                          mCanceled                   : 1;
  uint32_t                          mIsPending                  : 1;
  uint32_t                          mWasOpened                  : 1;
  // if 1 all "http-on-{opening|modify|etc}-request" observers have been called
  uint32_t                          mRequestObserversCalled     : 1;
  uint32_t                          mResponseHeadersModified    : 1;
  uint32_t                          mAllowPipelining            : 1;
  uint32_t                          mAllowSTS                   : 1;
  uint32_t                          mForceAllowThirdPartyCookie : 1;
  uint32_t                          mUploadStreamHasHeaders     : 1;
  uint32_t                          mInheritApplicationCache    : 1;
  uint32_t                          mChooseApplicationCache     : 1;
  uint32_t                          mLoadedFromApplicationCache : 1;
  uint32_t                          mChannelIsForDownload       : 1;
  uint32_t                          mTracingEnabled             : 1;
  // True if timing collection is enabled
  uint32_t                          mTimingEnabled              : 1;
  uint32_t                          mAllowSpdy                  : 1;
  uint32_t                          mLoadAsBlocking             : 1;
  uint32_t                          mLoadUnblocked              : 1;
  uint32_t                          mResponseTimeoutEnabled     : 1;
  // A flag that should be false only if a cross-domain redirect occurred
  uint32_t                          mAllRedirectsSameOrigin     : 1;

  // Current suspension depth for this channel object
  uint32_t                          mSuspendCount;

  nsCOMPtr<nsIURI>                  mAPIRedirectToURI;
  nsAutoPtr<nsTArray<nsCString> >   mRedirectedCachekeys;
  // Redirects added by previous channels.
  nsCOMArray<nsIPrincipal>          mRedirects;

  uint32_t                          mProxyResolveFlags;
  nsCOMPtr<nsIURI>                  mProxyURI;

  uint32_t                          mContentDispositionHint;
  nsAutoPtr<nsString>               mContentDispositionFilename;

  nsRefPtr<nsHttpHandler>           mHttpHandler;  // keep gHttpHandler alive

  // Performance tracking
  // The initiator type (for this resource) - how was the resource referenced in
  // the HTML file.
  nsString                          mInitiatorType;
  // Number of redirects that has occurred.
  int16_t                           mRedirectCount;
  // A time value equal to the starting time of the fetch that initiates the
  // redirect.
  mozilla::TimeStamp                mRedirectStartTimeStamp;
  // A time value equal to the time immediately after receiving the last byte of
  // the response of the last redirect.
  mozilla::TimeStamp                mRedirectEndTimeStamp;

  PRTime                            mChannelCreationTime;
  TimeStamp                         mChannelCreationTimestamp;
  TimeStamp                         mAsyncOpenTime;
  TimeStamp                         mCacheReadStart;
  TimeStamp                         mCacheReadEnd;
  // copied from the transaction before we null out mTransaction
  // so that the timing can still be queried from OnStopRequest
  TimingStruct                      mTransactionTimings;

  nsCOMPtr<nsIPrincipal>            mPrincipal;

  bool                              mForcePending;
};

// Share some code while working around C++'s absurd inability to handle casting
// of member functions between base/derived types.
// - We want to store member function pointer to call at resume time, but one
//   such function--HandleAsyncAbort--we want to share between the
//   nsHttpChannel/HttpChannelChild.  Can't define it in base class, because
//   then we'd have to cast member function ptr between base/derived class
//   types.  Sigh...
template <class T>
class HttpAsyncAborter
{
public:
  HttpAsyncAborter(T *derived) : mThis(derived), mCallOnResume(0) {}

  // Aborts channel: calls OnStart/Stop with provided status, removes channel
  // from loadGroup.
  nsresult AsyncAbort(nsresult status);

  // Does most the actual work.
  void HandleAsyncAbort();

  // AsyncCall calls a member function asynchronously (via an event).
  // retval isn't refcounted and is set only when event was successfully
  // posted, the event is returned for the purpose of cancelling when needed
  nsresult AsyncCall(void (T::*funcPtr)(),
                     nsRunnableMethod<T> **retval = nullptr);
private:
  T *mThis;

protected:
  // Function to be called at resume time
  void (T::* mCallOnResume)(void);
};

template <class T>
nsresult HttpAsyncAborter<T>::AsyncAbort(nsresult status)
{
  PR_LOG(gHttpLog, 4,
         ("HttpAsyncAborter::AsyncAbort [this=%p status=%x]\n", mThis, status));

  mThis->mStatus = status;

  // if this fails?  Callers ignore our return value anyway....
  return AsyncCall(&T::HandleAsyncAbort);
}

// Each subclass needs to define its own version of this (which just calls this
// base version), else we wind up casting base/derived member function ptrs
template <class T>
inline void HttpAsyncAborter<T>::HandleAsyncAbort()
{
  NS_PRECONDITION(!mCallOnResume, "How did that happen?");

  if (mThis->mSuspendCount) {
    PR_LOG(gHttpLog, 4,
           ("Waiting until resume to do async notification [this=%p]\n", mThis));
    mCallOnResume = &T::HandleAsyncAbort;
    return;
  }

  mThis->DoNotifyListener();

  // finally remove ourselves from the load group.
  if (mThis->mLoadGroup)
    mThis->mLoadGroup->RemoveRequest(mThis, nullptr, mThis->mStatus);
}

template <class T>
nsresult HttpAsyncAborter<T>::AsyncCall(void (T::*funcPtr)(),
                                   nsRunnableMethod<T> **retval)
{
  nsresult rv;

  nsRefPtr<nsRunnableMethod<T> > event = NS_NewRunnableMethod(mThis, funcPtr);
  rv = NS_DispatchToCurrentThread(event);
  if (NS_SUCCEEDED(rv) && retval) {
    *retval = event;
  }

  return rv;
}

} // namespace net
} // namespace mozilla

#endif // mozilla_net_HttpBaseChannel_h
