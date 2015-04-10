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
#include "nsIClassOfService.h"
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
#include "nsIHttpChannel.h"
#include "nsISecurityConsoleMessage.h"
#include "nsCOMArray.h"

extern PRLogModuleInfo *gHttpLog;
class nsPerformance;
class nsISecurityConsoleMessage;
class nsIPrincipal;

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
                      , public nsIClassOfService
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
  NS_IMETHOD GetName(nsACString& aName) override;
  NS_IMETHOD IsPending(bool *aIsPending) override;
  NS_IMETHOD GetStatus(nsresult *aStatus) override;
  NS_IMETHOD GetLoadGroup(nsILoadGroup **aLoadGroup) override;
  NS_IMETHOD SetLoadGroup(nsILoadGroup *aLoadGroup) override;
  NS_IMETHOD GetLoadFlags(nsLoadFlags *aLoadFlags) override;
  NS_IMETHOD SetLoadFlags(nsLoadFlags aLoadFlags) override;

  // nsIChannel
  NS_IMETHOD GetOriginalURI(nsIURI **aOriginalURI) override;
  NS_IMETHOD SetOriginalURI(nsIURI *aOriginalURI) override;
  NS_IMETHOD GetURI(nsIURI **aURI) override;
  NS_IMETHOD GetOwner(nsISupports **aOwner) override;
  NS_IMETHOD SetOwner(nsISupports *aOwner) override;
  NS_IMETHOD GetLoadInfo(nsILoadInfo **aLoadInfo) override;
  NS_IMETHOD SetLoadInfo(nsILoadInfo *aLoadInfo) override;
  NS_IMETHOD GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks) override;
  NS_IMETHOD SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks) override;
  NS_IMETHOD GetContentType(nsACString& aContentType) override;
  NS_IMETHOD SetContentType(const nsACString& aContentType) override;
  NS_IMETHOD GetContentCharset(nsACString& aContentCharset) override;
  NS_IMETHOD SetContentCharset(const nsACString& aContentCharset) override;
  NS_IMETHOD GetContentDisposition(uint32_t *aContentDisposition) override;
  NS_IMETHOD SetContentDisposition(uint32_t aContentDisposition) override;
  NS_IMETHOD GetContentDispositionFilename(nsAString& aContentDispositionFilename) override;
  NS_IMETHOD SetContentDispositionFilename(const nsAString& aContentDispositionFilename) override;
  NS_IMETHOD GetContentDispositionHeader(nsACString& aContentDispositionHeader) override;
  NS_IMETHOD GetContentLength(int64_t *aContentLength) override;
  NS_IMETHOD SetContentLength(int64_t aContentLength) override;
  NS_IMETHOD Open(nsIInputStream **aResult) override;

  // nsIEncodedChannel
  NS_IMETHOD GetApplyConversion(bool *value) override;
  NS_IMETHOD SetApplyConversion(bool value) override;
  NS_IMETHOD GetContentEncodings(nsIUTF8StringEnumerator** aEncodings) override;
  NS_IMETHOD DoApplyContentConversions(nsIStreamListener *aNextListener,
                                       nsIStreamListener **aNewNextListener,
                                       nsISupports *aCtxt) override;

  // HttpBaseChannel::nsIHttpChannel
  NS_IMETHOD GetRequestMethod(nsACString& aMethod) override;
  NS_IMETHOD SetRequestMethod(const nsACString& aMethod) override;
  NS_IMETHOD GetReferrer(nsIURI **referrer) override;
  NS_IMETHOD SetReferrer(nsIURI *referrer) override;
  NS_IMETHOD GetReferrerPolicy(uint32_t *referrerPolicy) override;
  NS_IMETHOD SetReferrerWithPolicy(nsIURI *referrer, uint32_t referrerPolicy) override;
  NS_IMETHOD GetRequestHeader(const nsACString& aHeader, nsACString& aValue) override;
  NS_IMETHOD SetRequestHeader(const nsACString& aHeader,
                              const nsACString& aValue, bool aMerge) override;
  NS_IMETHOD VisitRequestHeaders(nsIHttpHeaderVisitor *visitor) override;
  NS_IMETHOD GetResponseHeader(const nsACString &header, nsACString &value) override;
  NS_IMETHOD SetResponseHeader(const nsACString& header,
                               const nsACString& value, bool merge) override;
  NS_IMETHOD VisitResponseHeaders(nsIHttpHeaderVisitor *visitor) override;
  NS_IMETHOD GetAllowPipelining(bool *value) override;
  NS_IMETHOD SetAllowPipelining(bool value) override;
  NS_IMETHOD GetAllowSTS(bool *value) override;
  NS_IMETHOD SetAllowSTS(bool value) override;
  NS_IMETHOD GetRedirectionLimit(uint32_t *value) override;
  NS_IMETHOD SetRedirectionLimit(uint32_t value) override;
  NS_IMETHOD IsNoStoreResponse(bool *value) override;
  NS_IMETHOD IsNoCacheResponse(bool *value) override;
  NS_IMETHOD IsPrivateResponse(bool *value) override;
  NS_IMETHOD GetResponseStatus(uint32_t *aValue) override;
  NS_IMETHOD GetResponseStatusText(nsACString& aValue) override;
  NS_IMETHOD GetRequestSucceeded(bool *aValue) override;
  NS_IMETHOD RedirectTo(nsIURI *newURI) override;

  // nsIHttpChannelInternal
  NS_IMETHOD GetDocumentURI(nsIURI **aDocumentURI) override;
  NS_IMETHOD SetDocumentURI(nsIURI *aDocumentURI) override;
  NS_IMETHOD GetRequestVersion(uint32_t *major, uint32_t *minor) override;
  NS_IMETHOD GetResponseVersion(uint32_t *major, uint32_t *minor) override;
  NS_IMETHOD SetCookie(const char *aCookieHeader) override;
  NS_IMETHOD GetThirdPartyFlags(uint32_t *aForce) override;
  NS_IMETHOD SetThirdPartyFlags(uint32_t aForce) override;
  NS_IMETHOD GetForceAllowThirdPartyCookie(bool *aForce) override;
  NS_IMETHOD SetForceAllowThirdPartyCookie(bool aForce) override;
  NS_IMETHOD GetCanceled(bool *aCanceled) override;
  NS_IMETHOD GetChannelIsForDownload(bool *aChannelIsForDownload) override;
  NS_IMETHOD SetChannelIsForDownload(bool aChannelIsForDownload) override;
  NS_IMETHOD SetCacheKeysRedirectChain(nsTArray<nsCString> *cacheKeys) override;
  NS_IMETHOD GetLocalAddress(nsACString& addr) override;
  NS_IMETHOD GetLocalPort(int32_t* port) override;
  NS_IMETHOD GetRemoteAddress(nsACString& addr) override;
  NS_IMETHOD GetRemotePort(int32_t* port) override;
  NS_IMETHOD GetAllowSpdy(bool *aAllowSpdy) override;
  NS_IMETHOD SetAllowSpdy(bool aAllowSpdy) override;
  NS_IMETHOD GetApiRedirectToURI(nsIURI * *aApiRedirectToURI) override;
  nsresult AddSecurityMessage(const nsAString &aMessageTag, const nsAString &aMessageCategory);
  NS_IMETHOD TakeAllSecurityMessages(nsCOMArray<nsISecurityConsoleMessage> &aMessages) override;
  NS_IMETHOD GetResponseTimeoutEnabled(bool *aEnable) override;
  NS_IMETHOD SetResponseTimeoutEnabled(bool aEnable) override;
  NS_IMETHOD GetNetworkInterfaceId(nsACString& aNetworkInterfaceId) override;
  NS_IMETHOD SetNetworkInterfaceId(const nsACString& aNetworkInterfaceId) override;
  NS_IMETHOD AddRedirect(nsIPrincipal *aRedirect) override;
  NS_IMETHOD ForcePending(bool aForcePending) override;
  NS_IMETHOD GetLastModifiedTime(PRTime* lastModifiedTime) override;
  NS_IMETHOD ForceNoIntercept() override;
  NS_IMETHOD GetCorsIncludeCredentials(bool* aInclude) override;
  NS_IMETHOD SetCorsIncludeCredentials(bool aInclude) override;
  NS_IMETHOD GetCorsMode(uint32_t* aCorsMode) override;
  NS_IMETHOD SetCorsMode(uint32_t aCorsMode) override;
  NS_IMETHOD GetTopWindowURI(nsIURI **aTopWindowURI) override;
  NS_IMETHOD ContinueBeginConnect() override;
  NS_IMETHOD GetProxyURI(nsIURI **proxyURI) override;

  inline void CleanRedirectCacheChainIfNecessary()
  {
      mRedirectedCachekeys = nullptr;
  }
  NS_IMETHOD HTTPUpgrade(const nsACString & aProtocolName,
                         nsIHttpUpgradeListener *aListener) override;

  // nsISupportsPriority
  NS_IMETHOD GetPriority(int32_t *value) override;
  NS_IMETHOD AdjustPriority(int32_t delta) override;

  // nsIClassOfService
  NS_IMETHOD GetClassFlags(uint32_t *outFlags) override { *outFlags = mClassOfService; return NS_OK; }

  // nsIResumableChannel
  NS_IMETHOD GetEntityID(nsACString& aEntityID) override;

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

    nsresult OverrideSecurityInfo(nsISupports* aSecurityInfo);

public: /* Necko internal use only... */
    bool IsNavigation();

    // Return whether upon a redirect code of httpStatus for method, the
    // request method should be rewritten to GET.
    static bool ShouldRewriteRedirectToGET(uint32_t httpStatus,
                                           nsHttpRequestHead::ParsedMethodType method);

    // Like nsIEncodedChannel::DoApplyConversions except context is set to
    // mListenerContext.
    nsresult DoApplyContentConversions(nsIStreamListener *aNextListener,
                                       nsIStreamListener **aNewNextListener);

protected:
  nsCOMArray<nsISecurityConsoleMessage> mSecurityConsoleMessages;

  // Handle notifying listener, removing from loadgroup if request failed.
  void     DoNotifyListener();
  virtual void DoNotifyListenerCleanup() = 0;

  // drop reference to listener, its callbacks, and the progress sink
  void ReleaseListeners();

  nsPerformance* GetPerformance();

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

  // GetPrincipal Returns the channel's URI principal.
  nsIPrincipal *GetURIPrincipal();

  // Returns true if this channel should intercept the network request and prepare
  // for a possible synthesized response instead.
  bool ShouldIntercept();

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
  nsCOMPtr<nsISupports>             mSecurityInfo;

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
  uint32_t                          mClassOfService;
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
  uint32_t                          mThirdPartyFlags            : 3;
  uint32_t                          mUploadStreamHasHeaders     : 1;
  uint32_t                          mInheritApplicationCache    : 1;
  uint32_t                          mChooseApplicationCache     : 1;
  uint32_t                          mLoadedFromApplicationCache : 1;
  uint32_t                          mChannelIsForDownload       : 1;
  uint32_t                          mTracingEnabled             : 1;
  // True if timing collection is enabled
  uint32_t                          mTimingEnabled              : 1;
  uint32_t                          mAllowSpdy                  : 1;
  uint32_t                          mResponseTimeoutEnabled     : 1;
  // A flag that should be false only if a cross-domain redirect occurred
  uint32_t                          mAllRedirectsSameOrigin     : 1;

  // Is 1 if no redirects have occured or if all redirects
  // pass the Resource Timing timing-allow-check
  uint32_t                          mAllRedirectsPassTimingAllowCheck : 1;

  // True if this channel should skip any interception checks
  uint32_t                          mForceNoIntercept           : 1;

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

  uint32_t                          mReferrerPolicy;

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
  nsCOMPtr<nsIURI>                  mTopWindowURI;

  bool mCorsIncludeCredentials;
  uint32_t mCorsMode;

  // This parameter is used to ensure that we do not call OnStartRequest more
  // than once.
  bool mOnStartRequestCalled;

  // The network interface id that's associated with this channel.
  nsCString mNetworkInterfaceId;
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
  explicit HttpAsyncAborter(T *derived) : mThis(derived), mCallOnResume(0) {}

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
