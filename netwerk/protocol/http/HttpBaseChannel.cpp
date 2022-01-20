/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "mozilla/net/HttpBaseChannel.h"

#include <algorithm>
#include <utility>

#include "HttpBaseChannel.h"
#include "HttpLog.h"
#include "LoadInfo.h"
#include "ReferrerInfo.h"
#include "mozIThirdPartyUtil.h"
#include "mozilla/AntiTrackingUtils.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/BinarySearch.h"
#include "mozilla/ConsoleReportCollector.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/InputStreamLengthHelper.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/Components.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_fission.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/nsHTTPSOnlyUtils.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/dom/PerformanceStorage.h"
#include "mozilla/dom/ProcessIsolation.h"
#include "mozilla/dom/WindowGlobalParent.h"
#include "mozilla/net/OpaqueResponseUtils.h"
#include "mozilla/net/PartiallySeekableInputStream.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/UrlClassifierFeatureFactory.h"
#include "nsCRT.h"
#include "nsContentSecurityManager.h"
#include "nsContentSecurityUtils.h"
#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsGlobalWindowOuter.h"
#include "nsHttpChannel.h"
#include "nsHTTPCompressConv.h"
#include "nsHttpHandler.h"
#include "nsICacheInfoChannel.h"
#include "nsICachingChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIConsoleService.h"
#include "nsIContentPolicy.h"
#include "nsICookieService.h"
#include "nsIDOMWindowUtils.h"
#include "nsIDocShell.h"
#include "nsIDNSService.h"
#include "nsIEncodedChannel.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsILoadGroupChild.h"
#include "nsIMIMEInputStream.h"
#include "nsIMutableArray.h"
#include "nsINetworkInterceptController.h"
#include "nsIObserverService.h"
#include "nsIPrincipal.h"
#include "nsIProtocolProxyService.h"
#include "nsIScriptError.h"
#include "nsIScriptSecurityManager.h"
#include "nsISecurityConsoleMessage.h"
#include "nsISeekableStream.h"
#include "nsIStorageStream.h"
#include "nsIStreamConverterService.h"
#include "nsITimedChannel.h"
#include "nsITransportSecurityInfo.h"
#include "nsIURIMutator.h"
#include "nsMimeTypes.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"
#include "nsPIDOMWindow.h"
#include "nsProxyRelease.h"
#include "nsReadableUtils.h"
#include "nsRedirectHistoryEntry.h"
#include "nsServerTiming.h"
#include "nsStreamListenerWrapper.h"
#include "nsStreamUtils.h"
#include "nsThreadUtils.h"
#include "nsURLHelper.h"
#include "mozilla/RemoteLazyInputStreamChild.h"
#include "mozilla/RemoteLazyInputStreamUtils.h"
#include "mozilla/net/SFVService.h"
#include "mozilla/dom/ContentChild.h"
#include "nsQueryObject.h"

namespace mozilla {
namespace net {

static bool IsHeaderBlacklistedForRedirectCopy(nsHttpAtom const& aHeader) {
  // IMPORTANT: keep this list ASCII-code sorted
  static nsHttpAtom const* blackList[] = {&nsHttp::Accept,
                                          &nsHttp::Accept_Encoding,
                                          &nsHttp::Accept_Language,
                                          &nsHttp::Alternate_Service_Used,
                                          &nsHttp::Authentication,
                                          &nsHttp::Authorization,
                                          &nsHttp::Connection,
                                          &nsHttp::Content_Length,
                                          &nsHttp::Cookie,
                                          &nsHttp::Host,
                                          &nsHttp::If,
                                          &nsHttp::If_Match,
                                          &nsHttp::If_Modified_Since,
                                          &nsHttp::If_None_Match,
                                          &nsHttp::If_None_Match_Any,
                                          &nsHttp::If_Range,
                                          &nsHttp::If_Unmodified_Since,
                                          &nsHttp::Proxy_Authenticate,
                                          &nsHttp::Proxy_Authorization,
                                          &nsHttp::Range,
                                          &nsHttp::TE,
                                          &nsHttp::Transfer_Encoding,
                                          &nsHttp::Upgrade,
                                          &nsHttp::User_Agent,
                                          &nsHttp::WWW_Authenticate};

  class HttpAtomComparator {
    nsHttpAtom const& mTarget;

   public:
    explicit HttpAtomComparator(nsHttpAtom const& aTarget) : mTarget(aTarget) {}
    int operator()(nsHttpAtom const* aVal) const {
      if (mTarget == *aVal) {
        return 0;
      }
      return strcmp(mTarget.get(), aVal->get());
    }
  };

  size_t unused;
  return BinarySearchIf(blackList, 0, ArrayLength(blackList),
                        HttpAtomComparator(aHeader), &unused);
}

class AddHeadersToChannelVisitor final : public nsIHttpHeaderVisitor {
 public:
  NS_DECL_ISUPPORTS

  explicit AddHeadersToChannelVisitor(nsIHttpChannel* aChannel)
      : mChannel(aChannel) {}

  NS_IMETHOD VisitHeader(const nsACString& aHeader,
                         const nsACString& aValue) override {
    nsHttpAtom atom = nsHttp::ResolveAtom(aHeader);
    if (!IsHeaderBlacklistedForRedirectCopy(atom)) {
      DebugOnly<nsresult> rv =
          mChannel->SetRequestHeader(aHeader, aValue, false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
    return NS_OK;
  }

 private:
  ~AddHeadersToChannelVisitor() = default;

  nsCOMPtr<nsIHttpChannel> mChannel;
};

NS_IMPL_ISUPPORTS(AddHeadersToChannelVisitor, nsIHttpHeaderVisitor)

HttpBaseChannel::HttpBaseChannel()
    : mReportCollector(new ConsoleReportCollector()),
      mHttpHandler(gHttpHandler),
      mChannelCreationTime(0),
      mComputedCrossOriginOpenerPolicy(nsILoadInfo::OPENER_POLICY_UNSAFE_NONE),
      mStartPos(UINT64_MAX),
      mTransferSize(0),
      mRequestSize(0),
      mDecodedBodySize(0),
      mSupportsHTTP3(false),
      mEncodedBodySize(0),
      mRequestContextID(0),
      mContentWindowId(0),
      mTopBrowsingContextId(0),
      mAltDataLength(-1),
      mChannelId(0),
      mReqContentLength(0U),
      mStatus(NS_OK),
      mCanceled(false),
      mFirstPartyClassificationFlags(0),
      mThirdPartyClassificationFlags(0),
      mFlashPluginState(nsIHttpChannel::FlashPluginUnknown),
      mLoadFlags(LOAD_NORMAL),
      mCaps(0),
      mClassOfService(0),
      mTlsFlags(0),
      mSuspendCount(0),
      mInitialRwin(0),
      mProxyResolveFlags(0),
      mContentDispositionHint(UINT32_MAX),
      mCorsMode(nsIHttpChannelInternal::CORS_MODE_NO_CORS),
      mRedirectMode(nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW),
      mLastRedirectFlags(0),
      mPriority(PRIORITY_NORMAL),
      mRedirectionLimit(gHttpHandler->RedirectionLimit()),
      mRedirectCount(0),
      mInternalRedirectCount(0),
      mCachedOpaqueResponseBlockingPref(
          StaticPrefs::browser_opaqueResponseBlocking()),
      mBlockOpaqueResponseAfterSniff(false),
      mCheckIsOpaqueResponseAllowedAfterSniff(false),
      mDummyChannelForImageCache(false) {
  StoreApplyConversion(true);
  StoreAllowSTS(true);
  StoreTracingEnabled(true);
  StoreReportTiming(true);
  StoreAllowSpdy(true);
  StoreAllowHttp3(true);
  StoreAllowAltSvc(true);
  StoreResponseTimeoutEnabled(true);
  StoreAllRedirectsSameOrigin(true);
  StoreAllRedirectsPassTimingAllowCheck(true);
  StoreUpgradableToSecure(true);

  this->mSelfAddr.inet = {};
  this->mPeerAddr.inet = {};
  LOG(("Creating HttpBaseChannel @%p\n", this));

  // Subfields of unions cannot be targeted in an initializer list.
#ifdef MOZ_VALGRIND
  // Zero the entire unions so that Valgrind doesn't complain when we send them
  // to another process.
  memset(&mSelfAddr, 0, sizeof(NetAddr));
  memset(&mPeerAddr, 0, sizeof(NetAddr));
#endif
  mSelfAddr.raw.family = PR_AF_UNSPEC;
  mPeerAddr.raw.family = PR_AF_UNSPEC;
}

HttpBaseChannel::~HttpBaseChannel() {
  LOG(("Destroying HttpBaseChannel @%p\n", this));

  // Make sure we don't leak
  CleanRedirectCacheChainIfNecessary();

  ReleaseMainThreadOnlyReferences();
}

namespace {  // anon

class NonTailRemover : public nsISupports {
  NS_DECL_THREADSAFE_ISUPPORTS

  explicit NonTailRemover(nsIRequestContext* rc) : mRequestContext(rc) {}

 private:
  virtual ~NonTailRemover() {
    MOZ_ASSERT(NS_IsMainThread());
    mRequestContext->RemoveNonTailRequest();
  }

  nsCOMPtr<nsIRequestContext> mRequestContext;
};

NS_IMPL_ISUPPORTS0(NonTailRemover)

}  // namespace

void HttpBaseChannel::ReleaseMainThreadOnlyReferences() {
  if (NS_IsMainThread()) {
    // Already on main thread, let dtor to
    // take care of releasing references
    RemoveAsNonTailRequest();
    return;
  }

  nsTArray<nsCOMPtr<nsISupports>> arrayToRelease;
  arrayToRelease.AppendElement(mLoadGroup.forget());
  arrayToRelease.AppendElement(mCallbacks.forget());
  arrayToRelease.AppendElement(mProgressSink.forget());
  arrayToRelease.AppendElement(mPrincipal.forget());
  arrayToRelease.AppendElement(mListener.forget());
  arrayToRelease.AppendElement(mCompressListener.forget());

  if (LoadAddedAsNonTailRequest()) {
    // RemoveNonTailRequest() on our request context must be called on the main
    // thread
    MOZ_RELEASE_ASSERT(mRequestContext,
                       "Someone released rc or set flags w/o having it?");

    nsCOMPtr<nsISupports> nonTailRemover(new NonTailRemover(mRequestContext));
    arrayToRelease.AppendElement(nonTailRemover.forget());
  }

  NS_DispatchToMainThread(new ProxyReleaseRunnable(std::move(arrayToRelease)));
}

void HttpBaseChannel::AddClassificationFlags(uint32_t aClassificationFlags,
                                             bool aIsThirdParty) {
  LOG(
      ("HttpBaseChannel::AddClassificationFlags classificationFlags=%d "
       "thirdparty=%d %p",
       aClassificationFlags, static_cast<int>(aIsThirdParty), this));

  if (aIsThirdParty) {
    mThirdPartyClassificationFlags |= aClassificationFlags;
  } else {
    mFirstPartyClassificationFlags |= aClassificationFlags;
  }
}

void HttpBaseChannel::SetFlashPluginState(
    nsIHttpChannel::FlashPluginState aState) {
  LOG(("HttpBaseChannel::SetFlashPluginState %p", this));
  mFlashPluginState = aState;
}

nsresult HttpBaseChannel::Init(nsIURI* aURI, uint32_t aCaps,
                               nsProxyInfo* aProxyInfo,
                               uint32_t aProxyResolveFlags, nsIURI* aProxyURI,
                               uint64_t aChannelId,
                               ExtContentPolicyType aContentPolicyType) {
  LOG1(("HttpBaseChannel::Init [this=%p]\n", this));

  MOZ_ASSERT(aURI, "null uri");

  mURI = aURI;
  mOriginalURI = aURI;
  mDocumentURI = nullptr;
  mCaps = aCaps;
  mProxyResolveFlags = aProxyResolveFlags;
  mProxyURI = aProxyURI;
  mChannelId = aChannelId;

  // Construct connection info object
  nsAutoCString host;
  int32_t port = -1;
  bool isHTTPS = mURI->SchemeIs("https");

  nsresult rv = mURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  // Reject the URL if it doesn't specify a host
  if (host.IsEmpty()) return NS_ERROR_MALFORMED_URI;

  rv = mURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  LOG1(("host=%s port=%d\n", host.get(), port));

  rv = mURI->GetAsciiSpec(mSpec);
  if (NS_FAILED(rv)) return rv;
  LOG1(("uri=%s\n", mSpec.get()));

  // Assert default request method
  MOZ_ASSERT(mRequestHead.EqualsMethod(nsHttpRequestHead::kMethod_Get));

  // Set request headers
  nsAutoCString hostLine;
  rv = nsHttpHandler::GenerateHostPort(host, port, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = mRequestHead.SetHeader(nsHttp::Host, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = gHttpHandler->AddStandardRequestHeaders(&mRequestHead, isHTTPS,
                                               aContentPolicyType);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString type;
  if (aProxyInfo && NS_SUCCEEDED(aProxyInfo->GetType(type)) &&
      !type.EqualsLiteral("unknown")) {
    mProxyInfo = aProxyInfo;
  }

  mCurrentThread = GetCurrentEventTarget();
  return rv;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(HttpBaseChannel)
NS_IMPL_RELEASE(HttpBaseChannel)

NS_INTERFACE_MAP_BEGIN(HttpBaseChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIIdentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIEncodedChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY(nsIForcePendingChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
  NS_INTERFACE_MAP_ENTRY(nsIFormPOSTActionChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel2)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateBrowsingChannel)
  NS_INTERFACE_MAP_ENTRY(nsITimedChannel)
  NS_INTERFACE_MAP_ENTRY(nsIConsoleReportCollector)
  NS_INTERFACE_MAP_ENTRY(nsIThrottledInputChannel)
  NS_INTERFACE_MAP_ENTRY(nsIClassifiedChannel)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(HttpBaseChannel)
NS_INTERFACE_MAP_END_INHERITING(nsHashPropertyBag)

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetName(nsACString& aName) {
  aName = mSpec;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsPending(bool* aIsPending) {
  NS_ENSURE_ARG_POINTER(aIsPending);
  *aIsPending = LoadIsPending() || LoadForcePending();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetStatus(nsresult* aStatus) {
  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = do_AddRef(mLoadGroup).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");

  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  mProgressSink = nullptr;
  UpdatePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  NS_ENSURE_ARG_POINTER(aLoadFlags);
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  if (!LoadIsOCSP()) {
    return GetTRRModeImpl(aTRRMode);
  }

  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  nsIDNSService::ResolverMode trrMode = nsIDNSService::MODE_NATIVEONLY;
  // If this is an OCSP channel, and the global TRR mode is TRR_ONLY (3)
  // then we set the mode for this channel as TRR_DISABLED_MODE.
  // We do this to prevent a TRR service channel's OCSP validation from
  // blocking DNS resolution completely.
  if (dns && NS_SUCCEEDED(dns->GetCurrentTrrMode(&trrMode)) &&
      trrMode == nsIDNSService::MODE_TRRONLY) {
    *aTRRMode = nsIRequest::TRR_DISABLED_MODE;
    return NS_OK;
  }

  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
HttpBaseChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
HttpBaseChannel::SetDocshellUserAgentOverride() {
  RefPtr<dom::BrowsingContext> bc;
  MOZ_ALWAYS_SUCCEEDS(mLoadInfo->GetBrowsingContext(getter_AddRefs(bc)));
  if (!bc) {
    return NS_OK;
  }

  nsAutoString customUserAgent;
  bc->GetCustomUserAgent(customUserAgent);
  if (customUserAgent.IsEmpty() || customUserAgent.IsVoid()) {
    return NS_OK;
  }

  NS_ConvertUTF16toUTF8 utf8CustomUserAgent(customUserAgent);
  nsresult rv = SetRequestHeader("User-Agent"_ns, utf8CustomUserAgent, false);
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  NS_ENSURE_ARG_POINTER(aOriginalURI);
  *aOriginalURI = do_AddRef(mOriginalURI).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  ENSURE_CALLED_BEFORE_CONNECT();

  NS_ENSURE_ARG_POINTER(aOriginalURI);
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetURI(nsIURI** aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = do_AddRef(mURI).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetOwner(nsISupports** aOwner) {
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = do_AddRef(mOwner).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetOwner(nsISupports* aOwner) {
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  *aLoadInfo = do_AddRef(mLoadInfo).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
HttpBaseChannel::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks) {
  *aCallbacks = do_AddRef(mCallbacks).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks) {
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");

  if (!CanSetCallbacks(aCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aCallbacks;
  mProgressSink = nullptr;

  UpdatePrivateBrowsing();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentType(nsACString& aContentType) {
  if (!mResponseHead) {
    aContentType.Truncate();
    return NS_ERROR_NOT_AVAILABLE;
  }

  mResponseHead->ContentType(aContentType);
  if (!aContentType.IsEmpty()) {
    return NS_OK;
  }

  aContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentType(const nsACString& aContentType) {
  if (mListener || LoadWasOpened() || mDummyChannelForImageCache) {
    if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

    nsAutoCString contentTypeBuf, charsetBuf;
    bool hadCharset;
    net_ParseContentType(aContentType, contentTypeBuf, charsetBuf, &hadCharset);

    mResponseHead->SetContentType(contentTypeBuf);

    // take care not to stomp on an existing charset
    if (hadCharset) mResponseHead->SetContentCharset(charsetBuf);

  } else {
    // We are being given a content-type hint.
    bool dummy;
    net_ParseContentType(aContentType, mContentTypeHint, mContentCharsetHint,
                         &dummy);
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentCharset(nsACString& aContentCharset) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

  mResponseHead->ContentCharset(aContentCharset);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentCharset(const nsACString& aContentCharset) {
  if (mListener) {
    if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

    mResponseHead->SetContentCharset(aContentCharset);
  } else {
    // Charset hint
    mContentCharsetHint = aContentCharset;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  // See bug 1658877. If mContentDispositionHint is already
  // DISPOSITION_ATTACHMENT, it means this channel is created from a
  // download attribute. In this case, we should prefer the value from the
  // download attribute rather than the value in content disposition header.
  // DISPOSITION_FORCE_INLINE is used to explicitly set inline, used by
  // the pdf reader when loading a attachment pdf without having to
  // download it.
  if (mContentDispositionHint == nsIChannel::DISPOSITION_ATTACHMENT ||
      mContentDispositionHint == nsIChannel::DISPOSITION_FORCE_INLINE) {
    *aContentDisposition = mContentDispositionHint;
    return NS_OK;
  }

  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_FAILED(rv)) {
    if (mContentDispositionHint == UINT32_MAX) return rv;

    *aContentDisposition = mContentDispositionHint;
    return NS_OK;
  }

  *aContentDisposition = NS_GetContentDispositionFromHeader(header, this);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentDisposition(uint32_t aContentDisposition) {
  mContentDispositionHint = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  aContentDispositionFilename.Truncate();
  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_SUCCEEDED(rv)) {
    rv = NS_GetFilenameFromDisposition(aContentDispositionFilename, header);
  }

  // If we failed to get the filename from header, we should use
  // mContentDispositionFilename, since mContentDispositionFilename is set from
  // the download attribute.
  if (NS_FAILED(rv)) {
    if (!mContentDispositionFilename) {
      return rv;
    }

    aContentDispositionFilename = *mContentDispositionFilename;
    return NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  mContentDispositionFilename =
      MakeUnique<nsString>(aContentDispositionFilename);

  // For safety reasons ensure the filename doesn't contain null characters and
  // replace them with underscores. We may later pass the extension to system
  // MIME APIs that expect null terminated strings.
  mContentDispositionFilename->ReplaceChar(char16_t(0), '_');

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = mResponseHead->GetHeader(nsHttp::Content_Disposition,
                                         aContentDispositionHeader);
  if (NS_FAILED(rv) || aContentDispositionHeader.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentLength(int64_t* aContentLength) {
  NS_ENSURE_ARG_POINTER(aContentLength);

  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

  if (LoadDeliveringAltData()) {
    MOZ_ASSERT(!mAvailableCachedAltDataType.IsEmpty());
    *aContentLength = mAltDataLength;
    return NS_OK;
  }

  *aContentLength = mResponseHead->ContentLength();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentLength(int64_t value) {
  if (!mDummyChannelForImageCache) {
    MOZ_ASSERT_UNREACHABLE("HttpBaseChannel::SetContentLength");
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  MOZ_ASSERT(mResponseHead);
  mResponseHead->SetContentLength(value);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::Open(nsIInputStream** aStream) {
  if (!gHttpHandler->Active()) {
    LOG(("HttpBaseChannel::Open after HTTP shutdown..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ENSURE_TRUE(!LoadWasOpened(), NS_ERROR_IN_PROGRESS);

  if (!gHttpHandler->Active()) {
    LOG(("HttpBaseChannel::Open after HTTP shutdown..."));
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_ImplementChannelOpen(this, aStream);
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIUploadChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetUploadStream(nsIInputStream** stream) {
  NS_ENSURE_ARG_POINTER(stream);
  *stream = do_AddRef(mUploadStream).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetUploadStream(nsIInputStream* stream,
                                 const nsACString& contentTypeArg,
                                 int64_t contentLength) {
  // NOTE: for backwards compatibility and for compatibility with old style
  // plugins, |stream| may include headers, specifically Content-Type and
  // Content-Length headers.  in this case, |contentType| and |contentLength|
  // would be unspecified.  this is traditionally the case of a POST request,
  // and so we select POST as the request method if contentType and
  // contentLength are unspecified.

  if (stream) {
    nsAutoCString method;
    bool hasHeaders = false;

    // This method and ExplicitSetUploadStream mean different things by "empty
    // content type string".  This method means "no header", but
    // ExplicitSetUploadStream means "header with empty value".  So we have to
    // massage the contentType argument into the form ExplicitSetUploadStream
    // expects.
    nsCOMPtr<nsIMIMEInputStream> mimeStream;
    nsCString contentType(contentTypeArg);
    if (contentType.IsEmpty()) {
      contentType.SetIsVoid(true);
      method = "POST"_ns;

      // MIME streams are a special case, and include headers which need to be
      // copied to the channel.
      mimeStream = do_QueryInterface(stream);
      if (mimeStream) {
        // Copy non-origin related headers to the channel.
        nsCOMPtr<nsIHttpHeaderVisitor> visitor =
            new AddHeadersToChannelVisitor(this);
        mimeStream->VisitHeaders(visitor);

        return ExplicitSetUploadStream(stream, contentType, contentLength,
                                       method, hasHeaders);
      }

      hasHeaders = true;
    } else {
      method = "PUT"_ns;

      MOZ_ASSERT(
          NS_FAILED(CallQueryInterface(stream, getter_AddRefs(mimeStream))),
          "nsIMIMEInputStream should not be set with an explicit content type");
    }
    return ExplicitSetUploadStream(stream, contentType, contentLength, method,
                                   hasHeaders);
  }

  // if stream is null, ExplicitSetUploadStream returns error.
  // So we need special case for GET method.
  StoreUploadStreamHasHeaders(false);
  mRequestHead.SetMethod("GET"_ns);  // revert to GET request
  mUploadStream = stream;
  return NS_OK;
}

namespace {

void CopyComplete(void* aClosure, nsresult aStatus) {
#ifdef DEBUG
  // Called on the STS thread by NS_AsyncCopy
  nsCOMPtr<nsIEventTarget> sts =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
  bool result = false;
  sts->IsOnCurrentThread(&result);
  MOZ_ASSERT(result, "Should only be called on the STS thread.");
#endif

  auto* channel = static_cast<HttpBaseChannel*>(aClosure);
  channel->OnCopyComplete(aStatus);
}

}  // anonymous namespace

NS_IMETHODIMP
HttpBaseChannel::EnsureUploadStreamIsCloneable(nsIRunnable* aCallback) {
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  NS_ENSURE_ARG_POINTER(aCallback);

  // We could in theory allow multiple callers to use this method,
  // but the complexity does not seem worth it yet.  Just fail if
  // this is called more than once simultaneously.
  NS_ENSURE_FALSE(mUploadCloneableCallback, NS_ERROR_UNEXPECTED);

  // We can immediately exec the callback if we don't have an upload stream.
  if (!mUploadStream) {
    aCallback->Run();
    return NS_OK;
  }

  // Upload nsIInputStream must be cloneable and seekable in order to be
  // processed by devtools network inspector.
  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
  if (seekable && NS_InputStreamIsCloneable(mUploadStream)) {
    aCallback->Run();
    return NS_OK;
  }

  nsCOMPtr<nsIStorageStream> storageStream;
  nsresult rv =
      NS_NewStorageStream(4096, UINT32_MAX, getter_AddRefs(storageStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> newUploadStream;
  rv = storageStream->NewInputStream(0, getter_AddRefs(newUploadStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> sink;
  rv = storageStream->GetOutputStream(0, getter_AddRefs(sink));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> source;
  if (NS_InputStreamIsBuffered(mUploadStream)) {
    source = mUploadStream;
  } else {
    rv = NS_NewBufferedInputStream(getter_AddRefs(source),
                                   mUploadStream.forget(), 4096);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIEventTarget> target =
      do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  mUploadCloneableCallback = aCallback;

  rv = NS_AsyncCopy(source, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                    4096,  // copy segment size
                    CopyComplete, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mUploadCloneableCallback = nullptr;
    return rv;
  }

  // Since we're consuming the old stream, replace it with the new
  // stream immediately.
  mUploadStream = newUploadStream;

  // Explicity hold the stream alive until copying is complete.  This will
  // be released in EnsureUploadStreamIsCloneableComplete().
  AddRef();

  return NS_OK;
}

void HttpBaseChannel::OnCopyComplete(nsresult aStatus) {
  // Assert in parent process because we don't have to label the runnable
  // in parent process.
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIRunnable> runnable = NewRunnableMethod<nsresult>(
      "net::HttpBaseChannel::EnsureUploadStreamIsCloneableComplete", this,
      &HttpBaseChannel::EnsureUploadStreamIsCloneableComplete, aStatus);
  NS_DispatchToMainThread(runnable.forget());
}

void HttpBaseChannel::EnsureUploadStreamIsCloneableComplete(nsresult aStatus) {
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  MOZ_ASSERT(mUploadCloneableCallback);

  if (NS_SUCCEEDED(mStatus)) {
    mStatus = aStatus;
  }

  mUploadCloneableCallback->Run();
  mUploadCloneableCallback = nullptr;

  // Release the reference we grabbed in EnsureUploadStreamIsCloneable() now
  // that the copying is complete.
  Release();
}

NS_IMETHODIMP
HttpBaseChannel::CloneUploadStream(int64_t* aContentLength,
                                   nsIInputStream** aClonedStream) {
  NS_ENSURE_ARG_POINTER(aContentLength);
  NS_ENSURE_ARG_POINTER(aClonedStream);
  *aClonedStream = nullptr;

  if (!mUploadStream) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv =
      NS_CloneInputStream(mUploadStream, getter_AddRefs(clonedStream));
  NS_ENSURE_SUCCESS(rv, rv);

  clonedStream.forget(aClonedStream);

  *aContentLength = mReqContentLength;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIUploadChannel2
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::ExplicitSetUploadStream(nsIInputStream* aStream,
                                         const nsACString& aContentType,
                                         int64_t aContentLength,
                                         const nsACString& aMethod,
                                         bool aStreamHasHeaders) {
  // Ensure stream is set and method is valid
  NS_ENSURE_TRUE(aStream, NS_ERROR_FAILURE);

  {
    DebugOnly<nsCOMPtr<nsIMIMEInputStream>> mimeStream;
    MOZ_ASSERT(
        !aStreamHasHeaders || NS_FAILED(CallQueryInterface(
                                  aStream, getter_AddRefs(mimeStream.value))),
        "nsIMIMEInputStream should not include headers");
  }

  nsresult rv = SetRequestMethod(aMethod);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aStreamHasHeaders && !aContentType.IsVoid()) {
    if (aContentType.IsEmpty()) {
      SetEmptyRequestHeader("Content-Type"_ns);
    } else {
      SetRequestHeader("Content-Type"_ns, aContentType, false);
    }
  }

  StoreUploadStreamHasHeaders(aStreamHasHeaders);

  nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(aStream);
  if (!seekable) {
    nsCOMPtr<nsIInputStream> stream = aStream;
    seekable = new PartiallySeekableInputStream(stream.forget());
  }

  mUploadStream = do_QueryInterface(seekable);

  if (aContentLength >= 0) {
    ExplicitSetUploadStreamLength(aContentLength, aStreamHasHeaders);
    return NS_OK;
  }

  // Sync access to the stream length.
  int64_t length;
  if (InputStreamLengthHelper::GetSyncLength(aStream, &length)) {
    ExplicitSetUploadStreamLength(length >= 0 ? length : 0, aStreamHasHeaders);
    return NS_OK;
  }

  // Let's resolve the size of the stream.
  RefPtr<HttpBaseChannel> self = this;
  InputStreamLengthHelper::GetAsyncLength(
      aStream, [self, aStreamHasHeaders](int64_t aLength) {
        self->StorePendingInputStreamLengthOperation(false);
        self->ExplicitSetUploadStreamLength(aLength >= 0 ? aLength : 0,
                                            aStreamHasHeaders);
        self->MaybeResumeAsyncOpen();
      });
  StorePendingInputStreamLengthOperation(true);
  return NS_OK;
}

void HttpBaseChannel::ExplicitSetUploadStreamLength(uint64_t aContentLength,
                                                    bool aStreamHasHeaders) {
  // We already have the content length. We don't need to determinate it.
  mReqContentLength = aContentLength;

  if (aStreamHasHeaders) {
    return;
  }

  nsAutoCString header;
  header.AssignLiteral("Content-Length");

  // Maybe the content-length header has been already set.
  nsAutoCString value;
  nsresult rv = GetRequestHeader(header, value);
  if (NS_SUCCEEDED(rv) && !value.IsEmpty()) {
    return;
  }

  // SetRequestHeader propagates headers to chrome if HttpChannelChild
  MOZ_ASSERT(!LoadWasOpened());
  nsAutoCString contentLengthStr;
  contentLengthStr.AppendInt(aContentLength);
  SetRequestHeader(header, contentLengthStr, false);
}

NS_IMETHODIMP
HttpBaseChannel::GetUploadStreamHasHeaders(bool* hasHeaders) {
  NS_ENSURE_ARG(hasHeaders);

  *hasHeaders = LoadUploadStreamHasHeaders();
  return NS_OK;
}

bool HttpBaseChannel::MaybeWaitForUploadStreamLength(
    nsIStreamListener* aListener, nsISupports* aContext) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!LoadAsyncOpenWaitingForStreamLength(),
             "AsyncOpen() called twice?");

  if (!LoadPendingInputStreamLengthOperation()) {
    return false;
  }

  mListener = aListener;
  StoreAsyncOpenWaitingForStreamLength(true);
  return true;
}

void HttpBaseChannel::MaybeResumeAsyncOpen() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!LoadPendingInputStreamLengthOperation());

  if (!LoadAsyncOpenWaitingForStreamLength()) {
    return;
  }

  nsCOMPtr<nsIStreamListener> listener;
  listener.swap(mListener);

  StoreAsyncOpenWaitingForStreamLength(false);

  nsresult rv = AsyncOpen(listener);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    DoAsyncAbort(rv);
  }
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIEncodedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetApplyConversion(bool* value) {
  *value = LoadApplyConversion();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetApplyConversion(bool value) {
  LOG(("HttpBaseChannel::SetApplyConversion [this=%p value=%d]\n", this,
       value));
  StoreApplyConversion(value);
  return NS_OK;
}

nsresult HttpBaseChannel::DoApplyContentConversions(
    nsIStreamListener* aNextListener, nsIStreamListener** aNewNextListener) {
  return DoApplyContentConversions(aNextListener, aNewNextListener, nullptr);
}

// create a listener chain that looks like this
// http-channel -> decompressor (n times) -> InterceptFailedOnSTop ->
// channel-creator-listener
//
// we need to do this because not every decompressor has fully streamed output
// so may need a call to OnStopRequest to identify its completion state.. and if
// it creates an error there the channel status code needs to be updated before
// calling the terminal listener. Having the decompress do it via cancel() means
// channels cannot effectively be used in two contexts (specifically this one
// and a peek context for sniffing)
//
class InterceptFailedOnStop : public nsIStreamListener {
  virtual ~InterceptFailedOnStop() = default;
  nsCOMPtr<nsIStreamListener> mNext;
  HttpBaseChannel* mChannel;

 public:
  InterceptFailedOnStop(nsIStreamListener* arg, HttpBaseChannel* chan)
      : mNext(arg), mChannel(chan) {}
  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD OnStartRequest(nsIRequest* aRequest) override {
    return mNext->OnStartRequest(aRequest);
  }

  NS_IMETHOD OnStopRequest(nsIRequest* aRequest,
                           nsresult aStatusCode) override {
    if (NS_FAILED(aStatusCode) && NS_SUCCEEDED(mChannel->mStatus)) {
      LOG(("HttpBaseChannel::InterceptFailedOnStop %p seting status %" PRIx32,
           mChannel, static_cast<uint32_t>(aStatusCode)));
      mChannel->mStatus = aStatusCode;
    }
    return mNext->OnStopRequest(aRequest, aStatusCode);
  }

  NS_IMETHOD OnDataAvailable(nsIRequest* aRequest, nsIInputStream* aInputStream,
                             uint64_t aOffset, uint32_t aCount) override {
    return mNext->OnDataAvailable(aRequest, aInputStream, aOffset, aCount);
  }
};

NS_IMPL_ISUPPORTS(InterceptFailedOnStop, nsIStreamListener, nsIRequestObserver)

NS_IMETHODIMP
HttpBaseChannel::DoApplyContentConversions(nsIStreamListener* aNextListener,
                                           nsIStreamListener** aNewNextListener,
                                           nsISupports* aCtxt) {
  *aNewNextListener = nullptr;
  if (!mResponseHead || !aNextListener) {
    return NS_OK;
  }

  LOG(("HttpBaseChannel::DoApplyContentConversions [this=%p]\n", this));

  if (!LoadApplyConversion()) {
    LOG(("not applying conversion per ApplyConversion\n"));
    return NS_OK;
  }

  if (LoadHasAppliedConversion()) {
    LOG(("not applying conversion because HasAppliedConversion is true\n"));
    return NS_OK;
  }

  if (LoadDeliveringAltData()) {
    MOZ_ASSERT(!mAvailableCachedAltDataType.IsEmpty());
    LOG(("not applying conversion because delivering alt-data\n"));
    return NS_OK;
  }

  nsAutoCString contentEncoding;
  nsresult rv =
      mResponseHead->GetHeader(nsHttp::Content_Encoding, contentEncoding);
  if (NS_FAILED(rv) || contentEncoding.IsEmpty()) return NS_OK;

  nsCOMPtr<nsIStreamListener> nextListener =
      new InterceptFailedOnStop(aNextListener, this);

  // The encodings are listed in the order they were applied
  // (see rfc 2616 section 14.11), so they need to removed in reverse
  // order. This is accomplished because the converter chain ends up
  // being a stack with the last converter created being the first one
  // to accept the raw network data.

  char* cePtr = contentEncoding.BeginWriting();
  uint32_t count = 0;
  while (char* val = nsCRT::strtok(cePtr, HTTP_LWS ",", &cePtr)) {
    if (++count > 16) {
      // That's ridiculous. We only understand 2 different ones :)
      // but for compatibility with old code, we will just carry on without
      // removing the encodings
      LOG(("Too many Content-Encodings. Ignoring remainder.\n"));
      break;
    }

    if (gHttpHandler->IsAcceptableEncoding(val, mURI->SchemeIs("https"))) {
      RefPtr<nsHTTPCompressConv> converter = new nsHTTPCompressConv();
      nsAutoCString from(val);
      ToLowerCase(from);
      rv = converter->AsyncConvertData(from.get(), "uncompressed", nextListener,
                                       aCtxt);
      if (NS_FAILED(rv)) {
        LOG(("Unexpected failure of AsyncConvertData %s\n", val));
        return rv;
      }

      LOG(("converter removed '%s' content-encoding\n", val));
      if (Telemetry::CanRecordPrereleaseData()) {
        int mode = 0;
        if (from.EqualsLiteral("gzip") || from.EqualsLiteral("x-gzip")) {
          mode = 1;
        } else if (from.EqualsLiteral("deflate") ||
                   from.EqualsLiteral("x-deflate")) {
          mode = 2;
        } else if (from.EqualsLiteral("br")) {
          mode = 3;
        }
        Telemetry::Accumulate(Telemetry::HTTP_CONTENT_ENCODING, mode);
      }
      nextListener = converter;
    } else {
      if (val) LOG(("Unknown content encoding '%s', ignoring\n", val));
    }
  }
  *aNewNextListener = do_AddRef(nextListener).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentEncodings(nsIUTF8StringEnumerator** aEncodings) {
  if (!mResponseHead) {
    *aEncodings = nullptr;
    return NS_OK;
  }

  nsAutoCString encoding;
  Unused << mResponseHead->GetHeader(nsHttp::Content_Encoding, encoding);
  if (encoding.IsEmpty()) {
    *aEncodings = nullptr;
    return NS_OK;
  }
  RefPtr<nsContentEncodings> enumerator =
      new nsContentEncodings(this, encoding.get());
  enumerator.forget(aEncodings);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings <public>
//-----------------------------------------------------------------------------

HttpBaseChannel::nsContentEncodings::nsContentEncodings(
    nsIHttpChannel* aChannel, const char* aEncodingHeader)
    : mEncodingHeader(aEncodingHeader), mChannel(aChannel), mReady(false) {
  mCurEnd = aEncodingHeader + strlen(aEncodingHeader);
  mCurStart = mCurEnd;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings::nsISimpleEnumerator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::nsContentEncodings::HasMore(bool* aMoreEncodings) {
  if (mReady) {
    *aMoreEncodings = true;
    return NS_OK;
  }

  nsresult rv = PrepareForNext();
  *aMoreEncodings = NS_SUCCEEDED(rv);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::nsContentEncodings::GetNext(nsACString& aNextEncoding) {
  aNextEncoding.Truncate();
  if (!mReady) {
    nsresult rv = PrepareForNext();
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  const nsACString& encoding = Substring(mCurStart, mCurEnd);

  nsACString::const_iterator start, end;
  encoding.BeginReading(start);
  encoding.EndReading(end);

  bool haveType = false;
  if (CaseInsensitiveFindInReadable("gzip"_ns, start, end)) {
    aNextEncoding.AssignLiteral(APPLICATION_GZIP);
    haveType = true;
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable("compress"_ns, start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_COMPRESS);
      haveType = true;
    }
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable("deflate"_ns, start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_ZIP);
      haveType = true;
    }
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable("br"_ns, start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_BROTLI);
      haveType = true;
    }
  }

  // Prepare to fetch the next encoding
  mCurEnd = mCurStart;
  mReady = false;

  if (haveType) return NS_OK;

  NS_WARNING("Unknown encoding type");
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(HttpBaseChannel::nsContentEncodings, nsIUTF8StringEnumerator,
                  nsIStringEnumerator)

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings <private>
//-----------------------------------------------------------------------------

nsresult HttpBaseChannel::nsContentEncodings::PrepareForNext(void) {
  MOZ_ASSERT(mCurStart == mCurEnd, "Indeterminate state");

  // At this point both mCurStart and mCurEnd point to somewhere
  // past the end of the next thing we want to return

  while (mCurEnd != mEncodingHeader) {
    --mCurEnd;
    if (*mCurEnd != ',' && !nsCRT::IsAsciiSpace(*mCurEnd)) break;
  }
  if (mCurEnd == mEncodingHeader) {
    return NS_ERROR_NOT_AVAILABLE;  // no more encodings
  }
  ++mCurEnd;

  // At this point mCurEnd points to the first char _after_ the
  // header we want.  Furthermore, mCurEnd - 1 != mEncodingHeader

  mCurStart = mCurEnd - 1;
  while (mCurStart != mEncodingHeader && *mCurStart != ',' &&
         !nsCRT::IsAsciiSpace(*mCurStart)) {
    --mCurStart;
  }
  if (*mCurStart == ',' || nsCRT::IsAsciiSpace(*mCurStart)) {
    ++mCurStart;  // we stopped because of a weird char, so move up one
  }

  // At this point mCurStart and mCurEnd bracket the encoding string
  // we want.  Check that it's not "identity"
  if (Substring(mCurStart, mCurEnd)
          .Equals("identity", nsCaseInsensitiveCStringComparator)) {
    mCurEnd = mCurStart;
    return PrepareForNext();
  }

  mReady = true;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetChannelId(uint64_t* aChannelId) {
  NS_ENSURE_ARG_POINTER(aChannelId);
  *aChannelId = mChannelId;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetChannelId(uint64_t aChannelId) {
  mChannelId = aChannelId;
  return NS_OK;
}

NS_IMETHODIMP HttpBaseChannel::GetTopLevelContentWindowId(uint64_t* aWindowId) {
  if (!mContentWindowId) {
    nsCOMPtr<nsILoadContext> loadContext;
    GetCallback(loadContext);
    if (loadContext) {
      nsCOMPtr<mozIDOMWindowProxy> topWindow;
      loadContext->GetTopWindow(getter_AddRefs(topWindow));
      if (topWindow) {
        if (nsPIDOMWindowInner* inner =
                nsPIDOMWindowOuter::From(topWindow)->GetCurrentInnerWindow()) {
          mContentWindowId = inner->WindowID();
        }
      }
    }
  }
  *aWindowId = mContentWindowId;
  return NS_OK;
}

NS_IMETHODIMP HttpBaseChannel::SetTopBrowsingContextId(uint64_t aId) {
  mTopBrowsingContextId = aId;
  return NS_OK;
}

NS_IMETHODIMP HttpBaseChannel::GetTopBrowsingContextId(uint64_t* aId) {
  EnsureTopBrowsingContextId();
  *aId = mTopBrowsingContextId;
  return NS_OK;
}

NS_IMETHODIMP HttpBaseChannel::SetTopLevelContentWindowId(uint64_t aWindowId) {
  mContentWindowId = aWindowId;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsThirdPartyTrackingResource(bool* aIsTrackingResource) {
  MOZ_ASSERT(
      !(mFirstPartyClassificationFlags && mThirdPartyClassificationFlags));
  *aIsTrackingResource = UrlClassifierCommon::IsTrackingClassificationFlag(
      mThirdPartyClassificationFlags);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsThirdPartySocialTrackingResource(
    bool* aIsThirdPartySocialTrackingResource) {
  MOZ_ASSERT(!mFirstPartyClassificationFlags ||
             !mThirdPartyClassificationFlags);
  *aIsThirdPartySocialTrackingResource =
      UrlClassifierCommon::IsSocialTrackingClassificationFlag(
          mThirdPartyClassificationFlags);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetClassificationFlags(uint32_t* aFlags) {
  if (mThirdPartyClassificationFlags) {
    *aFlags = mThirdPartyClassificationFlags;
  } else {
    *aFlags = mFirstPartyClassificationFlags;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetFirstPartyClassificationFlags(uint32_t* aFlags) {
  *aFlags = mFirstPartyClassificationFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetThirdPartyClassificationFlags(uint32_t* aFlags) {
  *aFlags = mThirdPartyClassificationFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetFlashPluginState(nsIHttpChannel::FlashPluginState* aState) {
  uint32_t flashPluginState = mFlashPluginState;
  *aState = (nsIHttpChannel::FlashPluginState)flashPluginState;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTransferSize(uint64_t* aTransferSize) {
  *aTransferSize = mTransferSize;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestSize(uint64_t* aRequestSize) {
  *aRequestSize = mRequestSize;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetDecodedBodySize(uint64_t* aDecodedBodySize) {
  *aDecodedBodySize = mDecodedBodySize;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetEncodedBodySize(uint64_t* aEncodedBodySize) {
  *aEncodedBodySize = mEncodedBodySize;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetSupportsHTTP3(bool* aSupportsHTTP3) {
  *aSupportsHTTP3 = mSupportsHTTP3;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetHasHTTPSRR(bool* aHasHTTPSRR) {
  *aHasHTTPSRR = LoadHasHTTPSRR();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestMethod(nsACString& aMethod) {
  mRequestHead.Method(aMethod);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRequestMethod(const nsACString& aMethod) {
  ENSURE_CALLED_BEFORE_CONNECT();

  const nsCString& flatMethod = PromiseFlatCString(aMethod);

  // Method names are restricted to valid HTTP tokens.
  if (!nsHttp::IsValidToken(flatMethod)) return NS_ERROR_INVALID_ARG;

  mRequestHead.SetMethod(flatMethod);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  NS_ENSURE_ARG_POINTER(aReferrerInfo);
  *aReferrerInfo = do_AddRef(mReferrerInfo).take();
  return NS_OK;
}

nsresult HttpBaseChannel::SetReferrerInfoInternal(
    nsIReferrerInfo* aReferrerInfo, bool aClone, bool aCompute,
    bool aRespectBeforeConnect) {
  LOG(
      ("HttpBaseChannel::SetReferrerInfoInternal [this=%p aClone(%d) "
       "aCompute(%d)]\n",
       this, aClone, aCompute));
  if (aRespectBeforeConnect) {
    ENSURE_CALLED_BEFORE_CONNECT();
  }

  mReferrerInfo = aReferrerInfo;

  // clear existing referrer, if any
  nsresult rv = ClearReferrerHeader();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!mReferrerInfo) {
    return NS_OK;
  }

  if (aClone) {
    mReferrerInfo = static_cast<dom::ReferrerInfo*>(aReferrerInfo)->Clone();
  }

  dom::ReferrerInfo* referrerInfo =
      static_cast<dom::ReferrerInfo*>(mReferrerInfo.get());

  // Don't set referrerInfo if it has not been initialized.
  if (!referrerInfo->IsInitialized()) {
    mReferrerInfo = nullptr;
    return NS_ERROR_NOT_INITIALIZED;
  }

  if (aClone) {
    // Record the telemetry once we set the referrer info to the channel
    // successfully.
    referrerInfo->RecordTelemetry(this);
  }

  if (aCompute) {
    rv = referrerInfo->ComputeReferrer(this);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  nsCOMPtr<nsIURI> computedReferrer = mReferrerInfo->GetComputedReferrer();
  if (!computedReferrer) {
    return NS_OK;
  }

  nsAutoCString spec;
  rv = computedReferrer->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return SetReferrerHeader(spec, aRespectBeforeConnect);
}

NS_IMETHODIMP
HttpBaseChannel::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  return SetReferrerInfoInternal(aReferrerInfo, true, true, true);
}

NS_IMETHODIMP
HttpBaseChannel::SetReferrerInfoWithoutClone(nsIReferrerInfo* aReferrerInfo) {
  return SetReferrerInfoInternal(aReferrerInfo, false, true, true);
}

// Return the channel's proxy URI, or if it doesn't exist, the
// channel's main URI.
NS_IMETHODIMP
HttpBaseChannel::GetProxyURI(nsIURI** aOut) {
  NS_ENSURE_ARG_POINTER(aOut);
  nsCOMPtr<nsIURI> result(mProxyURI);
  result.forget(aOut);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestHeader(const nsACString& aHeader,
                                  nsACString& aValue) {
  aValue.Truncate();

  // XXX might be better to search the header list directly instead of
  // hitting the http atom hash table.
  nsHttpAtom atom = nsHttp::ResolveAtom(aHeader);
  if (!atom) return NS_ERROR_NOT_AVAILABLE;

  return mRequestHead.GetHeader(atom, aValue);
}

NS_IMETHODIMP
HttpBaseChannel::SetRequestHeader(const nsACString& aHeader,
                                  const nsACString& aValue, bool aMerge) {
  const nsCString& flatHeader = PromiseFlatCString(aHeader);
  const nsCString& flatValue = PromiseFlatCString(aValue);

  LOG(
      ("HttpBaseChannel::SetRequestHeader [this=%p header=\"%s\" value=\"%s\" "
       "merge=%u]\n",
       this, flatHeader.get(), flatValue.get(), aMerge));

  // Verify header names are valid HTTP tokens and header values are reasonably
  // close to whats allowed in RFC 2616.
  if (!nsHttp::IsValidToken(flatHeader) ||
      !nsHttp::IsReasonableHeaderValue(flatValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  return mRequestHead.SetHeader(aHeader, flatValue, aMerge);
}

NS_IMETHODIMP
HttpBaseChannel::SetNewReferrerInfo(const nsACString& aUrl,
                                    nsIReferrerInfo::ReferrerPolicyIDL aPolicy,
                                    bool aSendReferrer) {
  nsresult rv;
  // Create URI from string
  nsCOMPtr<nsIURI> aURI;
  rv = NS_NewURI(getter_AddRefs(aURI), aUrl);
  NS_ENSURE_SUCCESS(rv, rv);
  // Create new ReferrerInfo and initialize it.
  nsCOMPtr<nsIReferrerInfo> referrerInfo = new mozilla::dom::ReferrerInfo();
  rv = referrerInfo->Init(aPolicy, aSendReferrer, aURI);
  NS_ENSURE_SUCCESS(rv, rv);
  // Set ReferrerInfo
  return SetReferrerInfo(referrerInfo);
}

NS_IMETHODIMP
HttpBaseChannel::SetEmptyRequestHeader(const nsACString& aHeader) {
  const nsCString& flatHeader = PromiseFlatCString(aHeader);

  LOG(("HttpBaseChannel::SetEmptyRequestHeader [this=%p header=\"%s\"]\n", this,
       flatHeader.get()));

  // Verify header names are valid HTTP tokens and header values are reasonably
  // close to whats allowed in RFC 2616.
  if (!nsHttp::IsValidToken(flatHeader)) {
    return NS_ERROR_INVALID_ARG;
  }

  return mRequestHead.SetEmptyHeader(aHeader);
}

NS_IMETHODIMP
HttpBaseChannel::VisitRequestHeaders(nsIHttpHeaderVisitor* visitor) {
  return mRequestHead.VisitHeaders(visitor);
}

NS_IMETHODIMP
HttpBaseChannel::VisitNonDefaultRequestHeaders(nsIHttpHeaderVisitor* visitor) {
  return mRequestHead.VisitHeaders(visitor,
                                   nsHttpHeaderArray::eFilterSkipDefault);
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseHeader(const nsACString& header,
                                   nsACString& value) {
  value.Truncate();

  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

  nsHttpAtom atom = nsHttp::ResolveAtom(header);
  if (!atom) return NS_ERROR_NOT_AVAILABLE;

  return mResponseHead->GetHeader(atom, value);
}

NS_IMETHODIMP
HttpBaseChannel::SetResponseHeader(const nsACString& header,
                                   const nsACString& value, bool merge) {
  LOG(
      ("HttpBaseChannel::SetResponseHeader [this=%p header=\"%s\" value=\"%s\" "
       "merge=%u]\n",
       this, PromiseFlatCString(header).get(), PromiseFlatCString(value).get(),
       merge));

  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;

  nsHttpAtom atom = nsHttp::ResolveAtom(header);
  if (!atom) return NS_ERROR_NOT_AVAILABLE;

  // these response headers must not be changed
  if (atom == nsHttp::Content_Type || atom == nsHttp::Content_Length ||
      atom == nsHttp::Content_Encoding || atom == nsHttp::Trailer ||
      atom == nsHttp::Transfer_Encoding) {
    return NS_ERROR_ILLEGAL_VALUE;
  }

  StoreResponseHeadersModified(true);

  return mResponseHead->SetHeader(header, value, merge);
}

NS_IMETHODIMP
HttpBaseChannel::VisitResponseHeaders(nsIHttpHeaderVisitor* visitor) {
  if (!mResponseHead) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  return mResponseHead->VisitHeaders(visitor,
                                     nsHttpHeaderArray::eFilterResponse);
}

NS_IMETHODIMP
HttpBaseChannel::GetOriginalResponseHeader(const nsACString& aHeader,
                                           nsIHttpHeaderVisitor* aVisitor) {
  if (!mResponseHead) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsHttpAtom atom = nsHttp::ResolveAtom(aHeader);
  if (!atom) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mResponseHead->GetOriginalHeader(atom, aVisitor);
}

NS_IMETHODIMP
HttpBaseChannel::VisitOriginalResponseHeaders(nsIHttpHeaderVisitor* aVisitor) {
  if (!mResponseHead) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mResponseHead->VisitHeaders(
      aVisitor, nsHttpHeaderArray::eFilterResponseOriginal);
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowPipelining(bool* value) {
  NS_ENSURE_ARG_POINTER(value);
  *value = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowPipelining(bool value) {
  ENSURE_CALLED_BEFORE_CONNECT();
  // nop
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowSTS(bool* value) {
  NS_ENSURE_ARG_POINTER(value);
  *value = LoadAllowSTS();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowSTS(bool value) {
  ENSURE_CALLED_BEFORE_CONNECT();
  StoreAllowSTS(value);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIsOCSP(bool* value) {
  NS_ENSURE_ARG_POINTER(value);
  *value = LoadIsOCSP();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetIsOCSP(bool value) {
  ENSURE_CALLED_BEFORE_CONNECT();
  StoreIsOCSP(value);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectionLimit(uint32_t* value) {
  NS_ENSURE_ARG_POINTER(value);
  *value = mRedirectionLimit;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectionLimit(uint32_t value) {
  ENSURE_CALLED_BEFORE_CONNECT();

  mRedirectionLimit = std::min<uint32_t>(value, 0xff);
  return NS_OK;
}

nsresult HttpBaseChannel::OverrideSecurityInfo(nsISupports* aSecurityInfo) {
  MOZ_ASSERT(!mSecurityInfo,
             "This can only be called when we don't have a security info "
             "object already");
  MOZ_RELEASE_ASSERT(
      aSecurityInfo,
      "This can only be called with a valid security info object");
  MOZ_ASSERT(!BypassServiceWorker(),
             "This can only be called on channels that are not bypassing "
             "interception");
  MOZ_ASSERT(LoadResponseCouldBeSynthesized(),
             "This can only be called on channels that can be intercepted");
  if (mSecurityInfo) {
    LOG(
        ("HttpBaseChannel::OverrideSecurityInfo mSecurityInfo is null! "
         "[this=%p]\n",
         this));
    return NS_ERROR_UNEXPECTED;
  }
  if (!LoadResponseCouldBeSynthesized()) {
    LOG(
        ("HttpBaseChannel::OverrideSecurityInfo channel cannot be intercepted! "
         "[this=%p]\n",
         this));
    return NS_ERROR_UNEXPECTED;
  }

  mSecurityInfo = aSecurityInfo;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoStoreResponse(bool* value) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoStore();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoCacheResponse(bool* value) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoCache();
  if (!*value) *value = mResponseHead->ExpiresInPast();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsPrivateResponse(bool* value) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->Private();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStatus(uint32_t* aValue) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  *aValue = mResponseHead->Status();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStatusText(nsACString& aValue) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  mResponseHead->StatusText(aValue);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestSucceeded(bool* aValue) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  uint32_t status = mResponseHead->Status();
  *aValue = (status / 100 == 2);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::RedirectTo(nsIURI* targetURI) {
  NS_ENSURE_ARG(targetURI);

  nsAutoCString spec;
  targetURI->GetAsciiSpec(spec);
  LOG(("HttpBaseChannel::RedirectTo [this=%p, uri=%s]", this, spec.get()));
  LogCallingScriptLocation(this);

  // We cannot redirect after OnStartRequest of the listener
  // has been called, since to redirect we have to switch channels
  // and the dance with OnStartRequest et al has to start over.
  // This would break the nsIStreamListener contract.
  NS_ENSURE_FALSE(LoadOnStartRequestCalled(), NS_ERROR_NOT_AVAILABLE);

  mAPIRedirectToURI = targetURI;
  // Only Web Extensions are allowed to redirect a channel to a data:
  // URI. To avoid any bypasses after the channel was flagged by
  // the WebRequst API, we are dropping the flag here.
  mLoadInfo->SetAllowInsecureRedirectToDataURI(false);

  // We may want to rewrite origin allowance, hence we need an
  // artificial response head.
  if (!mResponseHead) {
    mResponseHead.reset(new nsHttpResponseHead());
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::UpgradeToSecure() {
  // Upgrades are handled internally between http-on-modify-request and
  // http-on-before-connect, which means upgrades are only possible during
  // on-modify, or WebRequest.onBeforeRequest in Web Extensions.  Once we are
  // past the code path where upgrades are handled, attempting an upgrade
  // will throw an error.
  NS_ENSURE_TRUE(LoadUpgradableToSecure(), NS_ERROR_NOT_AVAILABLE);

  StoreUpgradeToSecure(true);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestContextID(uint64_t* aRCID) {
  NS_ENSURE_ARG_POINTER(aRCID);
  *aRCID = mRequestContextID;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRequestContextID(uint64_t aRCID) {
  mRequestContextID = aRCID;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIsMainDocumentChannel(bool* aValue) {
  NS_ENSURE_ARG_POINTER(aValue);
  *aValue = IsNavigation();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetIsMainDocumentChannel(bool aValue) {
  StoreForceMainDocumentChannel(aValue);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetProtocolVersion(nsACString& aProtocolVersion) {
  nsresult rv;
  nsCOMPtr<nsITransportSecurityInfo> info =
      do_QueryInterface(mSecurityInfo, &rv);
  nsAutoCString protocol;
  if (NS_SUCCEEDED(rv) && info &&
      NS_SUCCEEDED(info->GetNegotiatedNPN(protocol)) && !protocol.IsEmpty()) {
    // The negotiated protocol was not empty so we can use it.
    aProtocolVersion = protocol;
    return NS_OK;
  }

  if (mResponseHead) {
    HttpVersion version = mResponseHead->Version();
    aProtocolVersion.Assign(nsHttp::GetProtocolVersion(version));
    return NS_OK;
  }

  return NS_ERROR_NOT_AVAILABLE;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetTopWindowURIIfUnknown(nsIURI* aTopWindowURI) {
  if (!aTopWindowURI) {
    return NS_ERROR_INVALID_ARG;
  }

  if (mTopWindowURI) {
    LOG(
        ("HttpChannelBase::SetTopWindowURIIfUnknown [this=%p] "
         "mTopWindowURI is already set.\n",
         this));
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIURI> topWindowURI;
  Unused << GetTopWindowURI(getter_AddRefs(topWindowURI));

  // Don't modify |mTopWindowURI| if we can get one from GetTopWindowURI().
  if (topWindowURI) {
    LOG(
        ("HttpChannelBase::SetTopWindowURIIfUnknown [this=%p] "
         "Return an error since we got a top window uri.\n",
         this));
    return NS_ERROR_FAILURE;
  }

  mTopWindowURI = aTopWindowURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTopWindowURI(nsIURI** aTopWindowURI) {
  nsCOMPtr<nsIURI> uriBeingLoaded =
      AntiTrackingUtils::MaybeGetDocumentURIBeingLoaded(this);
  return GetTopWindowURI(uriBeingLoaded, aTopWindowURI);
}

nsresult HttpBaseChannel::GetTopWindowURI(nsIURI* aURIBeingLoaded,
                                          nsIURI** aTopWindowURI) {
  nsresult rv = NS_OK;
  nsCOMPtr<mozIThirdPartyUtil> util;
  // Only compute the top window URI once. In e10s, this must be computed in the
  // child. The parent gets the top window URI through HttpChannelOpenArgs.
  if (!mTopWindowURI) {
    util = components::ThirdPartyUtil::Service();
    if (!util) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    nsCOMPtr<mozIDOMWindowProxy> win;
    rv = util->GetTopWindowForChannel(this, aURIBeingLoaded,
                                      getter_AddRefs(win));
    if (NS_SUCCEEDED(rv)) {
      rv = util->GetURIFromWindow(win, getter_AddRefs(mTopWindowURI));
#if DEBUG
      if (mTopWindowURI) {
        nsCString spec;
        if (NS_SUCCEEDED(mTopWindowURI->GetSpec(spec))) {
          LOG(("HttpChannelBase::Setting topwindow URI spec %s [this=%p]\n",
               spec.get(), this));
        }
      }
#endif
    }
  }
  *aTopWindowURI = do_AddRef(mTopWindowURI).take();
  return rv;
}

NS_IMETHODIMP
HttpBaseChannel::GetDocumentURI(nsIURI** aDocumentURI) {
  NS_ENSURE_ARG_POINTER(aDocumentURI);
  *aDocumentURI = do_AddRef(mDocumentURI).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetDocumentURI(nsIURI* aDocumentURI) {
  ENSURE_CALLED_BEFORE_CONNECT();

  mDocumentURI = aDocumentURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestVersion(uint32_t* major, uint32_t* minor) {
  HttpVersion version = mRequestHead.Version();

  if (major) {
    *major = static_cast<uint32_t>(version) / 10;
  }
  if (minor) {
    *minor = static_cast<uint32_t>(version) % 10;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseVersion(uint32_t* major, uint32_t* minor) {
  if (!mResponseHead) {
    *major = *minor = 0;  // we should at least be kind about it
    return NS_ERROR_NOT_AVAILABLE;
  }

  HttpVersion version = mResponseHead->Version();

  if (major) {
    *major = static_cast<uint32_t>(version) / 10;
  }
  if (minor) {
    *minor = static_cast<uint32_t>(version) % 10;
  }

  return NS_OK;
}

void HttpBaseChannel::NotifySetCookie(const nsACString& aCookie) {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->NotifyObservers(static_cast<nsIChannel*>(this),
                         "http-on-response-set-cookie",
                         NS_ConvertASCIItoUTF16(aCookie).get());
  }
}

bool HttpBaseChannel::IsBrowsingContextDiscarded() const {
  return mLoadGroup && mLoadGroup->GetIsBrowsingContextDiscarded();
}

// https://mikewest.github.io/corpp/#process-navigation-response
nsresult HttpBaseChannel::ProcessCrossOriginEmbedderPolicyHeader() {
  nsresult rv;
  if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    return NS_OK;
  }

  // Only consider Cross-Origin-Embedder-Policy for document loads.
  if (mLoadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_DOCUMENT &&
      mLoadInfo->GetExternalContentPolicyType() !=
          ExtContentPolicy::TYPE_SUBDOCUMENT) {
    return NS_OK;
  }

  nsILoadInfo::CrossOriginEmbedderPolicy resultPolicy =
      nsILoadInfo::EMBEDDER_POLICY_NULL;
  rv = GetResponseEmbedderPolicy(&resultPolicy);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  // https://mikewest.github.io/corpp/#abstract-opdef-process-navigation-response
  if (mLoadInfo->GetExternalContentPolicyType() ==
          ExtContentPolicy::TYPE_SUBDOCUMENT &&
      mLoadInfo->GetLoadingEmbedderPolicy() !=
          nsILoadInfo::EMBEDDER_POLICY_NULL &&
      resultPolicy != nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP) {
    return NS_ERROR_DOM_COEP_FAILED;
  }

  return NS_OK;
}

// https://mikewest.github.io/corpp/#corp-check
nsresult HttpBaseChannel::ProcessCrossOriginResourcePolicyHeader() {
  // Fetch 4.5.9
  uint32_t corsMode;
  MOZ_ALWAYS_SUCCEEDS(GetCorsMode(&corsMode));
  if (corsMode != nsIHttpChannelInternal::CORS_MODE_NO_CORS) {
    return NS_OK;
  }

  // We only apply this for resources.
  auto extContentPolicyType = mLoadInfo->GetExternalContentPolicyType();
  if (extContentPolicyType == ExtContentPolicy::TYPE_DOCUMENT ||
      extContentPolicyType == ExtContentPolicy::TYPE_WEBSOCKET ||
      extContentPolicyType == ExtContentPolicy::TYPE_SAVEAS_DOWNLOAD) {
    return NS_OK;
  }

  if (extContentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
    // COEP pref off, skip CORP checking for subdocument.
    if (!StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
      return NS_OK;
    }
    // COEP 3.2.1.2 when request targets a nested browsing context then embedder
    // policy value is "unsafe-none", then return allowed.
    if (mLoadInfo->GetLoadingEmbedderPolicy() ==
        nsILoadInfo::EMBEDDER_POLICY_NULL) {
      return NS_OK;
    }
  }

  MOZ_ASSERT(mLoadInfo->GetLoadingPrincipal(),
             "Resources should always have a LoadingPrincipal");
  if (!mResponseHead) {
    return NS_OK;
  }

  if (mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()) {
    return NS_OK;
  }

  nsAutoCString content;
  Unused << mResponseHead->GetHeader(nsHttp::Cross_Origin_Resource_Policy,
                                     content);

  if (StaticPrefs::browser_tabs_remote_useCrossOriginEmbedderPolicy()) {
    // COEP 3.2.1.6 If policy is null, and embedder policy is "require-corp",
    // set policy to "same-origin".
    // Note that we treat invalid value as "cross-origin", which spec
    // indicates. We might want to make that stricter.
    if (content.IsEmpty() && mLoadInfo->GetLoadingEmbedderPolicy() ==
                                 nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP) {
      content = "same-origin"_ns;
    }
  }

  if (content.IsEmpty()) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> channelOrigin;
  nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      this, getter_AddRefs(channelOrigin));

  // Cross-Origin-Resource-Policy = %s"same-origin" / %s"same-site" /
  // %s"cross-origin"
  if (content.EqualsLiteral("same-origin")) {
    if (!channelOrigin->Equals(mLoadInfo->GetLoadingPrincipal())) {
      return NS_ERROR_DOM_CORP_FAILED;
    }
    return NS_OK;
  }
  if (content.EqualsLiteral("same-site")) {
    nsAutoCString documentBaseDomain;
    nsAutoCString resourceBaseDomain;
    mLoadInfo->GetLoadingPrincipal()->GetBaseDomain(documentBaseDomain);
    channelOrigin->GetBaseDomain(resourceBaseDomain);
    if (documentBaseDomain != resourceBaseDomain) {
      return NS_ERROR_DOM_CORP_FAILED;
    }

    nsCOMPtr<nsIURI> resourceURI = channelOrigin->GetURI();
    if (!mLoadInfo->GetLoadingPrincipal()->SchemeIs("https") &&
        resourceURI->SchemeIs("https")) {
      return NS_ERROR_DOM_CORP_FAILED;
    }

    return NS_OK;
  }

  return NS_OK;
}

// See https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
// This method runs steps 1-4 of the algorithm to compare
// cross-origin-opener policies
static bool CompareCrossOriginOpenerPolicies(
    nsILoadInfo::CrossOriginOpenerPolicy documentPolicy,
    nsIPrincipal* documentOrigin,
    nsILoadInfo::CrossOriginOpenerPolicy resultPolicy,
    nsIPrincipal* resultOrigin) {
  if (documentPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE &&
      resultPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE) {
    return true;
  }

  if (documentPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE ||
      resultPolicy == nsILoadInfo::OPENER_POLICY_UNSAFE_NONE) {
    return false;
  }

  if (documentPolicy == resultPolicy && documentOrigin->Equals(resultOrigin)) {
    return true;
  }

  return false;
}

// This runs steps 1-5 of the algorithm when navigating a top level document.
// See https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
nsresult HttpBaseChannel::ComputeCrossOriginOpenerPolicyMismatch() {
  MOZ_ASSERT(XRE_IsParentProcess());

  StoreHasCrossOriginOpenerPolicyMismatch(false);
  if (!StaticPrefs::browser_tabs_remote_useCrossOriginOpenerPolicy()) {
    return NS_OK;
  }

  // Only consider Cross-Origin-Opener-Policy for toplevel document loads.
  if (mLoadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_DOCUMENT) {
    return NS_OK;
  }

  // Maybe the channel failed and we have no response head?
  if (!mResponseHead) {
    // Not having a response head is not a hard failure at the point where
    // this method is called.
    return NS_OK;
  }

  RefPtr<mozilla::dom::BrowsingContext> ctx;
  mLoadInfo->GetBrowsingContext(getter_AddRefs(ctx));

  // In xpcshell-tests we don't always have a browsingContext
  if (!ctx) {
    return NS_OK;
  }

  nsCOMPtr<nsIPrincipal> resultOrigin;
  nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
      this, getter_AddRefs(resultOrigin));

  // Get the policy of the active document, and the policy for the result.
  nsILoadInfo::CrossOriginOpenerPolicy documentPolicy = ctx->GetOpenerPolicy();
  nsILoadInfo::CrossOriginOpenerPolicy resultPolicy =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;
  Unused << ComputeCrossOriginOpenerPolicy(documentPolicy, &resultPolicy);
  mComputedCrossOriginOpenerPolicy = resultPolicy;

  // Add a permission to mark this site as high-value into the permission DB.
  if (resultPolicy != nsILoadInfo::OPENER_POLICY_UNSAFE_NONE) {
    mozilla::dom::AddHighValuePermission(
        resultOrigin, mozilla::dom::kHighValueCOOPPermission);
  }

  // If bc's popup sandboxing flag set is not empty and potentialCOOP is
  // non-null, then navigate bc to a network error and abort these steps.
  if (resultPolicy != nsILoadInfo::OPENER_POLICY_UNSAFE_NONE &&
      mLoadInfo->GetSandboxFlags()) {
    LOG((
        "HttpBaseChannel::ComputeCrossOriginOpenerPolicyMismatch network error "
        "for non empty sandboxing and non null COOP"));
    return NS_ERROR_DOM_COOP_FAILED;
  }

  // In xpcshell-tests we don't always have a current window global
  RefPtr<mozilla::dom::WindowGlobalParent> currentWindowGlobal =
      ctx->Canonical()->GetCurrentWindowGlobal();
  if (!currentWindowGlobal) {
    return NS_OK;
  }

  // We use the top window principal as the documentOrigin
  nsCOMPtr<nsIPrincipal> documentOrigin =
      currentWindowGlobal->DocumentPrincipal();

  bool compareResult = CompareCrossOriginOpenerPolicies(
      documentPolicy, documentOrigin, resultPolicy, resultOrigin);

  if (LOG_ENABLED()) {
    LOG(
        ("HttpBaseChannel::HasCrossOriginOpenerPolicyMismatch - "
         "doc:%d result:%d - compare:%d\n",
         documentPolicy, resultPolicy, compareResult));
    nsAutoCString docOrigin("(null)");
    nsCOMPtr<nsIURI> uri = documentOrigin->GetURI();
    if (uri) {
      uri->GetSpec(docOrigin);
    }
    nsAutoCString resOrigin("(null)");
    uri = resultOrigin->GetURI();
    if (uri) {
      uri->GetSpec(resOrigin);
    }
    LOG(("doc origin:%s - res origin: %s\n", docOrigin.get(), resOrigin.get()));
  }

  if (compareResult) {
    return NS_OK;
  }

  // If one of the following is false:
  //   - document's policy is same-origin-allow-popups
  //   - resultPolicy is null
  //   - doc is the initial about:blank document
  // then we have a mismatch.

  if (documentPolicy != nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS) {
    StoreHasCrossOriginOpenerPolicyMismatch(true);
    return NS_OK;
  }

  if (resultPolicy != nsILoadInfo::OPENER_POLICY_UNSAFE_NONE) {
    StoreHasCrossOriginOpenerPolicyMismatch(true);
    return NS_OK;
  }

  if (!currentWindowGlobal->IsInitialDocument()) {
    StoreHasCrossOriginOpenerPolicyMismatch(true);
    return NS_OK;
  }

  return NS_OK;
}

nsresult HttpBaseChannel::ProcessCrossOriginSecurityHeaders() {
  nsresult rv = ProcessCrossOriginEmbedderPolicyHeader();
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = ProcessCrossOriginResourcePolicyHeader();
  if (NS_FAILED(rv)) {
    return rv;
  }
  return ComputeCrossOriginOpenerPolicyMismatch();
}

enum class Report { Error, Warning };

// Helper Function to report messages to the console when the loaded
// script had a wrong MIME type.
void ReportMimeTypeMismatch(HttpBaseChannel* aChannel, const char* aMessageName,
                            nsIURI* aURI, const nsACString& aContentType,
                            Report report) {
  NS_ConvertUTF8toUTF16 spec(aURI->GetSpecOrDefault());
  NS_ConvertUTF8toUTF16 contentType(aContentType);

  aChannel->LogMimeTypeMismatch(nsCString(aMessageName),
                                report == Report::Warning, spec, contentType);
}

// Check and potentially enforce X-Content-Type-Options: nosniff
nsresult ProcessXCTO(HttpBaseChannel* aChannel, nsIURI* aURI,
                     nsHttpResponseHead* aResponseHead,
                     nsILoadInfo* aLoadInfo) {
  if (!aURI || !aResponseHead || !aLoadInfo) {
    // if there is no uri, no response head or no loadInfo, then there is
    // nothing to do
    return NS_OK;
  }

  // 1) Query the XCTO header and check if 'nosniff' is the first value.
  nsAutoCString contentTypeOptionsHeader;
  if (!aResponseHead->GetContentTypeOptionsHeader(contentTypeOptionsHeader)) {
    // if failed to get XCTO header, then there is nothing to do.
    return NS_OK;
  }

  // let's compare the header (ignoring case)
  // e.g. "NoSniFF" -> "nosniff"
  // if it's not 'nosniff' then there is nothing to do here
  if (!contentTypeOptionsHeader.EqualsIgnoreCase("nosniff")) {
    // since we are getting here, the XCTO header was sent;
    // a non matching value most likely means a mistake happenend;
    // e.g. sending 'nosnif' instead of 'nosniff', let's log a warning.
    AutoTArray<nsString, 1> params;
    CopyUTF8toUTF16(contentTypeOptionsHeader, *params.AppendElement());
    RefPtr<dom::Document> doc;
    aLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
    nsContentUtils::ReportToConsole(nsIScriptError::warningFlag, "XCTO"_ns, doc,
                                    nsContentUtils::eSECURITY_PROPERTIES,
                                    "XCTOHeaderValueMissing", params);
    return NS_OK;
  }

  // 2) Query the content type from the channel
  nsAutoCString contentType;
  aResponseHead->ContentType(contentType);

  // 3) Compare the expected MIME type with the actual type
  if (aLoadInfo->GetExternalContentPolicyType() ==
      ExtContentPolicy::TYPE_STYLESHEET) {
    if (contentType.EqualsLiteral(TEXT_CSS)) {
      return NS_OK;
    }
    ReportMimeTypeMismatch(aChannel, "MimeTypeMismatch2", aURI, contentType,
                           Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  if (aLoadInfo->GetExternalContentPolicyType() ==
      ExtContentPolicy::TYPE_SCRIPT) {
    if (nsContentUtils::IsJavascriptMIMEType(
            NS_ConvertUTF8toUTF16(contentType))) {
      return NS_OK;
    }
    ReportMimeTypeMismatch(aChannel, "MimeTypeMismatch2", aURI, contentType,
                           Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  auto policyType = aLoadInfo->GetExternalContentPolicyType();
  if (policyType == ExtContentPolicy::TYPE_DOCUMENT ||
      policyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
    // If the header XCTO nosniff is set for any browsing context, then
    // we set the skipContentSniffing flag on the Loadinfo. Within
    // GetMIMETypeFromContent we then bail early and do not do any sniffing.
    aLoadInfo->SetSkipContentSniffing(true);
    return NS_OK;
  }

  return NS_OK;
}

// Ensure that a load of type script has correct MIME type
nsresult EnsureMIMEOfScript(HttpBaseChannel* aChannel, nsIURI* aURI,
                            nsHttpResponseHead* aResponseHead,
                            nsILoadInfo* aLoadInfo) {
  if (!aURI || !aResponseHead || !aLoadInfo) {
    // if there is no uri, no response head or no loadInfo, then there is
    // nothing to do
    return NS_OK;
  }

  if (aLoadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_SCRIPT) {
    // if this is not a script load, then there is nothing to do
    return NS_OK;
  }

  nsAutoCString contentType;
  aResponseHead->ContentType(contentType);
  NS_ConvertUTF8toUTF16 typeString(contentType);

  if (nsContentUtils::IsJavascriptMIMEType(typeString)) {
    // script load has type script
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::javaScript);
    return NS_OK;
  }

  switch (aLoadInfo->InternalContentPolicyType()) {
    case nsIContentPolicy::TYPE_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_SCRIPT_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE:
    case nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD:
    case nsIContentPolicy::TYPE_INTERNAL_CHROMEUTILS_COMPILED_SCRIPT:
    case nsIContentPolicy::TYPE_INTERNAL_FRAME_MESSAGEMANAGER_SCRIPT:
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::script_load);
      break;
    case nsIContentPolicy::TYPE_INTERNAL_WORKER:
    case nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER:
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::worker_load);
      break;
    case nsIContentPolicy::TYPE_INTERNAL_SERVICE_WORKER:
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::serviceworker_load);
      break;
    case nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS:
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::importScript_load);
      break;
    case nsIContentPolicy::TYPE_INTERNAL_AUDIOWORKLET:
    case nsIContentPolicy::TYPE_INTERNAL_PAINTWORKLET:
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::worklet_load);
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unexpected script type");
      break;
  }

  bool isPrivateWin = aLoadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  bool isSameOrigin = false;
  aLoadInfo->GetLoadingPrincipal()->IsSameOrigin(aURI, isPrivateWin,
                                                 &isSameOrigin);
  if (isSameOrigin) {
    // same origin
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::same_origin);
  } else {
    bool cors = false;
    nsAutoCString corsOrigin;
    nsresult rv = aResponseHead->GetHeader(
        nsHttp::ResolveAtom("Access-Control-Allow-Origin"_ns), corsOrigin);
    if (NS_SUCCEEDED(rv)) {
      if (corsOrigin.Equals("*")) {
        cors = true;
      } else {
        nsCOMPtr<nsIURI> corsOriginURI;
        rv = NS_NewURI(getter_AddRefs(corsOriginURI), corsOrigin);
        if (NS_SUCCEEDED(rv)) {
          bool isPrivateWin =
              aLoadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
          bool isSameOrigin = false;
          aLoadInfo->GetLoadingPrincipal()->IsSameOrigin(
              corsOriginURI, isPrivateWin, &isSameOrigin);
          if (isSameOrigin) {
            cors = true;
          }
        }
      }
    }
    if (cors) {
      // cors origin
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::CORS_origin);
    } else {
      // cross origin
      AccumulateCategorical(
          Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::cross_origin);
    }
  }

  bool block = false;
  if (StringBeginsWith(contentType, "image/"_ns)) {
    // script load has type image
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::image);
    block = true;
  } else if (StringBeginsWith(contentType, "audio/"_ns)) {
    // script load has type audio
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::audio);
    block = true;
  } else if (StringBeginsWith(contentType, "video/"_ns)) {
    // script load has type video
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::video);
    block = true;
  } else if (StringBeginsWith(contentType, "text/csv"_ns)) {
    // script load has type text/csv
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::text_csv);
    block = true;
  }

  if (block) {
    ReportMimeTypeMismatch(aChannel, "BlockScriptWithWrongMimeType2", aURI,
                           contentType, Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  if (StringBeginsWith(contentType, "text/plain"_ns)) {
    // script load has type text/plain
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::text_plain);
  } else if (StringBeginsWith(contentType, "text/xml"_ns)) {
    // script load has type text/xml
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::text_xml);
  } else if (StringBeginsWith(contentType, "application/octet-stream"_ns)) {
    // script load has type application/octet-stream
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::app_octet_stream);
  } else if (StringBeginsWith(contentType, "application/xml"_ns)) {
    // script load has type application/xml
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::app_xml);
  } else if (StringBeginsWith(contentType, "application/json"_ns)) {
    // script load has type application/json
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::app_json);
  } else if (StringBeginsWith(contentType, "text/json"_ns)) {
    // script load has type text/json
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::text_json);
  } else if (StringBeginsWith(contentType, "text/html"_ns)) {
    // script load has type text/html
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::text_html);
  } else if (contentType.IsEmpty()) {
    // script load has no type
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::empty);
  } else {
    // script load has unknown type
    AccumulateCategorical(
        Telemetry::LABELS_SCRIPT_BLOCK_INCORRECT_MIME_3::unknown);
  }

  // We restrict importScripts() in worker code to JavaScript MIME types.
  nsContentPolicyType internalType = aLoadInfo->InternalContentPolicyType();
  if (internalType == nsIContentPolicy::TYPE_INTERNAL_WORKER_IMPORT_SCRIPTS) {
    ReportMimeTypeMismatch(aChannel, "BlockImportScriptsWithWrongMimeType",
                           aURI, contentType, Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  if (internalType == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
      internalType == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER) {
    // Do not block the load if the feature is not enabled.
    if (!StaticPrefs::security_block_Worker_with_wrong_mime()) {
      return NS_OK;
    }

    ReportMimeTypeMismatch(aChannel, "BlockWorkerWithWrongMimeType", aURI,
                           contentType, Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  // ES6 modules require a strict MIME type check.
  if (internalType == nsIContentPolicy::TYPE_INTERNAL_MODULE ||
      internalType == nsIContentPolicy::TYPE_INTERNAL_MODULE_PRELOAD) {
    ReportMimeTypeMismatch(aChannel, "BlockModuleWithWrongMimeType", aURI,
                           contentType, Report::Error);
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  return NS_OK;
}

// Warn when a load of type script uses a wrong MIME type and
// wasn't blocked by EnsureMIMEOfScript or ProcessXCTO.
void WarnWrongMIMEOfScript(HttpBaseChannel* aChannel, nsIURI* aURI,
                           nsHttpResponseHead* aResponseHead,
                           nsILoadInfo* aLoadInfo) {
  if (!aURI || !aResponseHead || !aLoadInfo) {
    // If there is no uri, no response head or no loadInfo, then there is
    // nothing to do.
    return;
  }

  if (aLoadInfo->GetExternalContentPolicyType() !=
      ExtContentPolicy::TYPE_SCRIPT) {
    // If this is not a script load, then there is nothing to do.
    return;
  }

  bool succeeded;
  MOZ_ALWAYS_SUCCEEDS(aChannel->GetRequestSucceeded(&succeeded));
  if (!succeeded) {
    // Do not warn for failed loads: HTTP error pages are usually in HTML.
    return;
  }

  nsAutoCString contentType;
  aResponseHead->ContentType(contentType);
  NS_ConvertUTF8toUTF16 typeString(contentType);
  if (!nsContentUtils::IsJavascriptMIMEType(typeString)) {
    ReportMimeTypeMismatch(aChannel, "WarnScriptWithWrongMimeType", aURI,
                           contentType, Report::Warning);
  }
}

nsresult HttpBaseChannel::ValidateMIMEType() {
  nsresult rv = EnsureMIMEOfScript(this, mURI, mResponseHead.get(), mLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = ProcessXCTO(this, mURI, mResponseHead.get(), mLoadInfo);
  if (NS_FAILED(rv)) {
    return rv;
  }

  WarnWrongMIMEOfScript(this, mURI, mResponseHead.get(), mLoadInfo);
  return NS_OK;
}

bool HttpBaseChannel::EnsureOpaqueResponseIsAllowed() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mCachedOpaqueResponseBlockingPref) {
    return true;
  }

  if (!mURI || !mResponseHead || !mLoadInfo) {
    // if there is no uri, no response head or no loadInfo, then there is
    // nothing to do
    return true;
  }

  nsCOMPtr<nsIPrincipal> principal = mLoadInfo->GetLoadingPrincipal();
  if (!principal || principal->IsSystemPrincipal()) {
    // If it's a top-level load or a system principal, then there is nothing to
    // do.
    return true;
  }

  // Check if the response is a opaque response, which means requestMode should
  // be RequestMode::No_cors and responseType should be ResponseType::Opaque.
  nsContentPolicyType contentPolicy = mLoadInfo->InternalContentPolicyType();
  // Skip the RequestMode would be RequestMode::Navigate
  if (contentPolicy == nsIContentPolicy::TYPE_DOCUMENT ||
      contentPolicy == nsIContentPolicy::TYPE_SUBDOCUMENT ||
      contentPolicy == nsIContentPolicy::TYPE_INTERNAL_FRAME ||
      contentPolicy == nsIContentPolicy::TYPE_INTERNAL_IFRAME ||
      // Skip the RequestMode would be RequestMode::Same_origin
      contentPolicy == nsIContentPolicy::TYPE_INTERNAL_WORKER ||
      contentPolicy == nsIContentPolicy::TYPE_INTERNAL_SHARED_WORKER) {
    return true;
  }

  uint32_t securityMode = mLoadInfo->GetSecurityMode();
  // Skip when RequestMode would not be RequestMode::no_cors
  if (securityMode !=
          nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_INHERITS_SEC_CONTEXT &&
      securityMode != nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL) {
    return true;
  }

  // Only continue when ResponseType would be ResponseType::Opaque
  if (mLoadInfo->GetTainting() != mozilla::LoadTainting::Opaque) {
    return true;
  }

  // Exclude object/embed element loading
  auto extContentPolicyType = mLoadInfo->GetExternalContentPolicyType();
  if (extContentPolicyType == ExtContentPolicy::TYPE_OBJECT ||
      extContentPolicyType == ExtContentPolicy::TYPE_OBJECT_SUBREQUEST) {
    return true;
  }

  // Ignore the request from object or embed elements
  if (mLoadInfo->GetIsFromObjectOrEmbed()) {
    return true;
  }

  InitiateORBTelemetry();

  nsAutoCString contentType;
  mResponseHead->ContentType(contentType);
  if (!contentType.IsEmpty()) {
    if (IsOpaqueSafeListedMIMEType(contentType)) {
      ReportORBTelemetry("Allowed_SafeListed"_ns);
      return true;
    }

    if (IsOpaqueBlockListedNeverSniffedMIMEType(contentType)) {
      // XXXtt: Report To Console.
      ReportORBTelemetry("Blocked_BlockListedNeverSniffed"_ns);
      return false;
    }

    if (mResponseHead->Status() == 206 &&
        IsOpaqueBlockListedMIMEType(contentType)) {
      // XXXtt: Report To Console.
      ReportORBTelemetry("Blocked_206AndBlockListed"_ns);
      return false;
    }

    nsAutoCString contentTypeOptionsHeader;
    if (mResponseHead->GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
        contentTypeOptionsHeader.EqualsIgnoreCase("nosniff") &&
        (IsOpaqueBlockListedMIMEType(contentType) ||
         contentType.EqualsLiteral(TEXT_PLAIN))) {
      // XXXtt: Report To Console.
      ReportORBTelemetry("Blocked_NosniffAndEitherBlockListedOrTextPlain"_ns);
      return false;
    }
  }

  // If it's a media subsequent request, we assume that it will only be made
  // after a successful initial request.
  bool isMediaRequest;
  mLoadInfo->GetIsMediaRequest(&isMediaRequest);
  if (isMediaRequest) {
    bool isMediaInitialRequest;
    mLoadInfo->GetIsMediaInitialRequest(&isMediaInitialRequest);
    if (!isMediaInitialRequest) {
      ReportORBTelemetry("Allowed_SubsequentMediaRequest"_ns);
      return true;
    }
  }

  if (mLoadFlags & nsIChannel::LOAD_CALL_CONTENT_SNIFFERS) {
    mSnifferCategoryType = SnifferCategoryType::All;
  } else {
    mSnifferCategoryType = SnifferCategoryType::OpaqueResponseBlocking;
  }

  mLoadFlags |= (nsIChannel::LOAD_CALL_CONTENT_SNIFFERS |
                 nsIChannel::LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE);
  mCheckIsOpaqueResponseAllowedAfterSniff = true;

  return true;
}

Result<bool, nsresult>
HttpBaseChannel::EnsureOpaqueResponseIsAllowedAfterSniff() {
  MOZ_ASSERT(XRE_IsParentProcess());

  if (!mCheckIsOpaqueResponseAllowedAfterSniff) {
    return true;
  }

  MOZ_ASSERT(mCachedOpaqueResponseBlockingPref);

  // Set mCheckIsOpaqueResponseAllowedAfterSniff as false to bypass
  // nsHttpChannel::DisableIsOpaqueResponseAllowedAfterSniff() when content
  // sniff happens in OnDataAvailable stage.
  //
  // XXXechuang: This method can return true when content type is
  // UNKNOWN_CONTENT_TYPE. But the response might be recognized as a blocked
  // type when after sniff in OnDataAvailable(). Should have a follow bug for
  // these cases.
  mCheckIsOpaqueResponseAllowedAfterSniff = false;

  if (mBlockOpaqueResponseAfterSniff) {
    // XXXtt: Report To Console.
    return false;
  }

  bool isMediaRequest;
  mLoadInfo->GetIsMediaRequest(&isMediaRequest);
  if (isMediaRequest) {
    // XXXtt: Report To Console.
    ReportORBTelemetry("Blocked_UnexpectedMediaRequest"_ns);
    return false;
  }

  nsAutoCString contentType;
  nsresult rv = GetContentType(contentType);
  if (NS_FAILED(rv)) {
    ReportORBTelemetry("Blocked_UnexpectedContentType"_ns);
    return Err(rv);
  }

  if (!mResponseHead) {
    ReportORBTelemetry("Allowed_UnexpectedResponseHead"_ns);
    return true;
  }

  nsAutoCString contentTypeOptionsHeader;
  if (mResponseHead->GetContentTypeOptionsHeader(contentTypeOptionsHeader) &&
      contentTypeOptionsHeader.EqualsIgnoreCase("nosniff")) {
    // XXXtt: Report To Console.
    ReportORBTelemetry("Blocked_NoSniffHeaderAfterSniff"_ns);
    return false;
  }

  if (mResponseHead->Status() < 200 || mResponseHead->Status() > 299) {
    // XXXtt: Report To Console.
    ReportORBTelemetry("Blocked_ResponseNotOk"_ns);
    return false;
  }

  if (contentType.EqualsLiteral(UNKNOWN_CONTENT_TYPE) ||
      contentType.EqualsLiteral(APPLICATION_OCTET_STREAM)) {
    ReportORBTelemetry("Allowed_FailtoGetMIMEType"_ns);
    return true;
  }

  if (StringBeginsWith(contentType, "image/"_ns) ||
      StringBeginsWith(contentType, "video/"_ns) ||
      StringBeginsWith(contentType, "audio/"_ns)) {
    // XXXtt: Report To Console.
    ReportORBTelemetry("Blocked_ContentTypeBeginsWithImageOrVideoOrAudio"_ns);
    return false;
  }

  // XXXtt: If response's body parses as JavaScript and does not parse as JSON,
  // then return true.

  int64_t contentLength;
  rv = GetContentLength(&contentLength);
  if (NS_FAILED(rv)) {
    // XXXtt: Report To Console.
    ReportORBTelemetry("Blocked_GetContentLengthFailed"_ns);
    return false;
  }

  ReportORBTelemetry(contentLength);
  ReportORBTelemetry("Allowed_NotImplementOrPass"_ns);

  return true;
}

NS_IMETHODIMP
HttpBaseChannel::SetCookie(const nsACString& aCookieHeader) {
  if (mLoadFlags & LOAD_ANONYMOUS) return NS_OK;

  if (IsBrowsingContextDiscarded()) {
    return NS_OK;
  }

  // empty header isn't an error
  if (aCookieHeader.IsEmpty()) {
    return NS_OK;
  }

  nsICookieService* cs = gHttpHandler->GetCookieService();
  NS_ENSURE_TRUE(cs, NS_ERROR_FAILURE);

  nsresult rv = cs->SetCookieStringFromHttp(mURI, aCookieHeader, this);
  if (NS_SUCCEEDED(rv)) {
    NotifySetCookie(aCookieHeader);
  }
  return rv;
}

NS_IMETHODIMP
HttpBaseChannel::GetThirdPartyFlags(uint32_t* aFlags) {
  *aFlags = LoadThirdPartyFlags();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetThirdPartyFlags(uint32_t aFlags) {
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  StoreThirdPartyFlags(aFlags);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetForceAllowThirdPartyCookie(bool* aForce) {
  *aForce = !!(LoadThirdPartyFlags() &
               nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetForceAllowThirdPartyCookie(bool aForce) {
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  if (aForce) {
    StoreThirdPartyFlags(LoadThirdPartyFlags() |
                         nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
  } else {
    StoreThirdPartyFlags(LoadThirdPartyFlags() &
                         ~nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = mCanceled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetChannelIsForDownload(bool* aChannelIsForDownload) {
  *aChannelIsForDownload = LoadChannelIsForDownload();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetChannelIsForDownload(bool aChannelIsForDownload) {
  StoreChannelIsForDownload(aChannelIsForDownload);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCacheKeysRedirectChain(nsTArray<nsCString>* cacheKeys) {
  mRedirectedCachekeys = WrapUnique(cacheKeys);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLocalAddress(nsACString& addr) {
  if (mSelfAddr.raw.family == PR_AF_UNSPEC) return NS_ERROR_NOT_AVAILABLE;

  addr.SetLength(kIPv6CStrBufSize);
  mSelfAddr.ToStringBuffer(addr.BeginWriting(), kIPv6CStrBufSize);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::TakeAllSecurityMessages(
    nsCOMArray<nsISecurityConsoleMessage>& aMessages) {
  MOZ_ASSERT(NS_IsMainThread());

  aMessages.Clear();
  for (const auto& pair : mSecurityConsoleMessages) {
    nsresult rv;
    nsCOMPtr<nsISecurityConsoleMessage> message =
        do_CreateInstance(NS_SECURITY_CONSOLE_MESSAGE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    message->SetTag(pair.first);
    message->SetCategory(pair.second);
    aMessages.AppendElement(message);
  }

  MOZ_ASSERT(mSecurityConsoleMessages.Length() == aMessages.Length());
  mSecurityConsoleMessages.Clear();

  return NS_OK;
}

/* Please use this method with care. This can cause the message
 * queue to grow large and cause the channel to take up a lot
 * of memory. Use only static string messages and do not add
 * server side data to the queue, as that can be large.
 * Add only a limited number of messages to the queue to keep
 * the channel size down and do so only in rare erroneous situations.
 * More information can be found here:
 * https://bugzilla.mozilla.org/show_bug.cgi?id=846918
 */
nsresult HttpBaseChannel::AddSecurityMessage(
    const nsAString& aMessageTag, const nsAString& aMessageCategory) {
  MOZ_ASSERT(NS_IsMainThread());

  nsresult rv;

  // nsSecurityConsoleMessage is not thread-safe refcounted.
  // Delay the object construction until requested.
  // See TakeAllSecurityMessages()
  std::pair<nsString, nsString> pair(aMessageTag, aMessageCategory);
  mSecurityConsoleMessages.AppendElement(std::move(pair));

  nsCOMPtr<nsIConsoleService> console(
      do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();

  auto innerWindowID = loadInfo->GetInnerWindowID();

  nsAutoString errorText;
  rv = nsContentUtils::GetLocalizedString(
      nsContentUtils::eSECURITY_PROPERTIES,
      NS_ConvertUTF16toUTF8(aMessageTag).get(), errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  error->InitWithSourceURI(
      errorText, mURI, u""_ns, 0, 0, nsIScriptError::warningFlag,
      NS_ConvertUTF16toUTF8(aMessageCategory), innerWindowID);

  console->LogMessage(error);

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLocalPort(int32_t* port) {
  NS_ENSURE_ARG_POINTER(port);

  if (mSelfAddr.raw.family == PR_AF_INET) {
    *port = (int32_t)ntohs(mSelfAddr.inet.port);
  } else if (mSelfAddr.raw.family == PR_AF_INET6) {
    *port = (int32_t)ntohs(mSelfAddr.inet6.port);
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRemoteAddress(nsACString& addr) {
  if (mPeerAddr.raw.family == PR_AF_UNSPEC) return NS_ERROR_NOT_AVAILABLE;

  addr.SetLength(kIPv6CStrBufSize);
  mPeerAddr.ToStringBuffer(addr.BeginWriting(), kIPv6CStrBufSize);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRemotePort(int32_t* port) {
  NS_ENSURE_ARG_POINTER(port);

  if (mPeerAddr.raw.family == PR_AF_INET) {
    *port = (int32_t)ntohs(mPeerAddr.inet.port);
  } else if (mPeerAddr.raw.family == PR_AF_INET6) {
    *port = (int32_t)ntohs(mPeerAddr.inet6.port);
  } else {
    return NS_ERROR_NOT_AVAILABLE;
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::HTTPUpgrade(const nsACString& aProtocolName,
                             nsIHttpUpgradeListener* aListener) {
  NS_ENSURE_ARG(!aProtocolName.IsEmpty());
  NS_ENSURE_ARG_POINTER(aListener);

  mUpgradeProtocol = aProtocolName;
  mUpgradeProtocolCallback = aListener;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetOnlyConnect(bool* aOnlyConnect) {
  NS_ENSURE_ARG_POINTER(aOnlyConnect);

  *aOnlyConnect = mCaps & NS_HTTP_CONNECT_ONLY;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetConnectOnly() {
  ENSURE_CALLED_BEFORE_CONNECT();

  if (!mUpgradeProtocolCallback) {
    return NS_ERROR_FAILURE;
  }

  mCaps |= NS_HTTP_CONNECT_ONLY;
  mProxyResolveFlags = nsIProtocolProxyService::RESOLVE_PREFER_HTTPS_PROXY |
                       nsIProtocolProxyService::RESOLVE_ALWAYS_TUNNEL;
  return SetLoadFlags(nsIRequest::INHIBIT_CACHING | nsIChannel::LOAD_ANONYMOUS |
                      nsIRequest::LOAD_BYPASS_CACHE |
                      nsIChannel::LOAD_BYPASS_SERVICE_WORKER);
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowSpdy(bool* aAllowSpdy) {
  NS_ENSURE_ARG_POINTER(aAllowSpdy);

  *aAllowSpdy = LoadAllowSpdy();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowSpdy(bool aAllowSpdy) {
  StoreAllowSpdy(aAllowSpdy);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowHttp3(bool* aAllowHttp3) {
  NS_ENSURE_ARG_POINTER(aAllowHttp3);

  *aAllowHttp3 = LoadAllowHttp3();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowHttp3(bool aAllowHttp3) {
  StoreAllowHttp3(aAllowHttp3);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowAltSvc(bool* aAllowAltSvc) {
  NS_ENSURE_ARG_POINTER(aAllowAltSvc);

  *aAllowAltSvc = LoadAllowAltSvc();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowAltSvc(bool aAllowAltSvc) {
  StoreAllowAltSvc(aAllowAltSvc);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetBeConservative(bool* aBeConservative) {
  NS_ENSURE_ARG_POINTER(aBeConservative);

  *aBeConservative = LoadBeConservative();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetBeConservative(bool aBeConservative) {
  StoreBeConservative(aBeConservative);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetBypassProxy(bool* aBypassProxy) {
  NS_ENSURE_ARG_POINTER(aBypassProxy);

  *aBypassProxy = LoadBypassProxy();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetBypassProxy(bool aBypassProxy) {
  StoreBypassProxy(aBypassProxy);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIsTRRServiceChannel(bool* aIsTRRServiceChannel) {
  NS_ENSURE_ARG_POINTER(aIsTRRServiceChannel);

  *aIsTRRServiceChannel = LoadIsTRRServiceChannel();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetIsTRRServiceChannel(bool aIsTRRServiceChannel) {
  StoreIsTRRServiceChannel(aIsTRRServiceChannel);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIsResolvedByTRR(bool* aResolvedByTRR) {
  NS_ENSURE_ARG_POINTER(aResolvedByTRR);
  *aResolvedByTRR = LoadResolvedByTRR();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTlsFlags(uint32_t* aTlsFlags) {
  NS_ENSURE_ARG_POINTER(aTlsFlags);

  *aTlsFlags = mTlsFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetTlsFlags(uint32_t aTlsFlags) {
  mTlsFlags = aTlsFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetApiRedirectToURI(nsIURI** aResult) {
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = do_AddRef(mAPIRedirectToURI).take();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseTimeoutEnabled(bool* aEnable) {
  if (NS_WARN_IF(!aEnable)) {
    return NS_ERROR_NULL_POINTER;
  }
  *aEnable = LoadResponseTimeoutEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetResponseTimeoutEnabled(bool aEnable) {
  StoreResponseTimeoutEnabled(aEnable);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetInitialRwin(uint32_t* aRwin) {
  if (NS_WARN_IF(!aRwin)) {
    return NS_ERROR_NULL_POINTER;
  }
  *aRwin = mInitialRwin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetInitialRwin(uint32_t aRwin) {
  ENSURE_CALLED_BEFORE_CONNECT();
  mInitialRwin = aRwin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::ForcePending(bool aForcePending) {
  StoreForcePending(aForcePending);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLastModifiedTime(PRTime* lastModifiedTime) {
  if (!mResponseHead) return NS_ERROR_NOT_AVAILABLE;
  uint32_t lastMod;
  nsresult rv = mResponseHead->GetLastModifiedValue(&lastMod);
  NS_ENSURE_SUCCESS(rv, rv);
  *lastModifiedTime = lastMod;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCorsIncludeCredentials(bool* aInclude) {
  *aInclude = LoadCorsIncludeCredentials();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCorsIncludeCredentials(bool aInclude) {
  StoreCorsIncludeCredentials(aInclude);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCorsMode(uint32_t* aMode) {
  *aMode = mCorsMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCorsMode(uint32_t aMode) {
  mCorsMode = aMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectMode(uint32_t* aMode) {
  *aMode = mRedirectMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectMode(uint32_t aMode) {
  mRedirectMode = aMode;
  return NS_OK;
}

namespace {

bool ContainsAllFlags(uint32_t aLoadFlags, uint32_t aMask) {
  return (aLoadFlags & aMask) == aMask;
}

}  // anonymous namespace

NS_IMETHODIMP
HttpBaseChannel::GetFetchCacheMode(uint32_t* aFetchCacheMode) {
  NS_ENSURE_ARG_POINTER(aFetchCacheMode);

  // Otherwise try to guess an appropriate cache mode from the load flags.
  if (ContainsAllFlags(mLoadFlags, INHIBIT_CACHING | LOAD_BYPASS_CACHE)) {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_STORE;
  } else if (ContainsAllFlags(mLoadFlags, LOAD_BYPASS_CACHE)) {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_RELOAD;
  } else if (ContainsAllFlags(mLoadFlags, VALIDATE_ALWAYS) ||
             LoadForceValidateCacheContent()) {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_CACHE;
  } else if (ContainsAllFlags(
                 mLoadFlags,
                 VALIDATE_NEVER | nsICachingChannel::LOAD_ONLY_FROM_CACHE)) {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_ONLY_IF_CACHED;
  } else if (ContainsAllFlags(mLoadFlags, VALIDATE_NEVER)) {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_FORCE_CACHE;
  } else {
    *aFetchCacheMode = nsIHttpChannelInternal::FETCH_CACHE_MODE_DEFAULT;
  }

  return NS_OK;
}

namespace {

void SetCacheFlags(uint32_t& aLoadFlags, uint32_t aFlags) {
  // First, clear any possible cache related flags.
  uint32_t allPossibleFlags =
      nsIRequest::INHIBIT_CACHING | nsIRequest::LOAD_BYPASS_CACHE |
      nsIRequest::VALIDATE_ALWAYS | nsIRequest::LOAD_FROM_CACHE |
      nsICachingChannel::LOAD_ONLY_FROM_CACHE;
  aLoadFlags &= ~allPossibleFlags;

  // Then set the new flags.
  aLoadFlags |= aFlags;
}

}  // anonymous namespace

NS_IMETHODIMP
HttpBaseChannel::SetFetchCacheMode(uint32_t aFetchCacheMode) {
  ENSURE_CALLED_BEFORE_CONNECT();

  // Now, set the load flags that implement each cache mode.
  switch (aFetchCacheMode) {
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_DEFAULT:
      // The "default" mode means to use the http cache normally and
      // respect any http cache-control headers.  We effectively want
      // to clear our cache related load flags.
      SetCacheFlags(mLoadFlags, 0);
      break;
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_STORE:
      // no-store means don't consult the cache on the way to the network, and
      // don't store the response in the cache even if it's cacheable.
      SetCacheFlags(mLoadFlags, INHIBIT_CACHING | LOAD_BYPASS_CACHE);
      break;
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_RELOAD:
      // reload means don't consult the cache on the way to the network, but
      // do store the response in the cache if possible.
      SetCacheFlags(mLoadFlags, LOAD_BYPASS_CACHE);
      break;
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_NO_CACHE:
      // no-cache means always validate what's in the cache.
      SetCacheFlags(mLoadFlags, VALIDATE_ALWAYS);
      break;
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_FORCE_CACHE:
      // force-cache means don't validate unless if the response would vary.
      SetCacheFlags(mLoadFlags, VALIDATE_NEVER);
      break;
    case nsIHttpChannelInternal::FETCH_CACHE_MODE_ONLY_IF_CACHED:
      // only-if-cached means only from cache, no network, no validation,
      // generate a network error if the document was't in the cache. The
      // privacy implications of these flags (making it fast/easy to check if
      // the user has things in their cache without any network traffic side
      // effects) are addressed in the Request constructor which
      // enforces/requires same-origin request mode.
      SetCacheFlags(mLoadFlags,
                    VALIDATE_NEVER | nsICachingChannel::LOAD_ONLY_FROM_CACHE);
      break;
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  uint32_t finalMode = 0;
  MOZ_ALWAYS_SUCCEEDS(GetFetchCacheMode(&finalMode));
  MOZ_DIAGNOSTIC_ASSERT(finalMode == aFetchCacheMode);
#endif  // MOZ_DIAGNOSTIC_ASSERT_ENABLED

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetIntegrityMetadata(const nsAString& aIntegrityMetadata) {
  mIntegrityMetadata = aIntegrityMetadata;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetIntegrityMetadata(nsAString& aIntegrityMetadata) {
  aIntegrityMetadata = mIntegrityMetadata;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetPriority(int32_t* value) {
  *value = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::AdjustPriority(int32_t delta) {
  return SetPriority(mPriority + delta);
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetEntityID(nsACString& aEntityID) {
  // Don't return an entity ID for Non-GET requests which require
  // additional data
  if (!mRequestHead.IsGet()) {
    return NS_ERROR_NOT_RESUMABLE;
  }

  uint64_t size = UINT64_MAX;
  nsAutoCString etag, lastmod;
  if (mResponseHead) {
    // Don't return an entity if the server sent the following header:
    // Accept-Ranges: none
    // Not sending the Accept-Ranges header means we can still try
    // sending range requests.
    nsAutoCString acceptRanges;
    Unused << mResponseHead->GetHeader(nsHttp::Accept_Ranges, acceptRanges);
    if (!acceptRanges.IsEmpty() &&
        !nsHttp::FindToken(acceptRanges.get(), "bytes",
                           HTTP_HEADER_VALUE_SEPS)) {
      return NS_ERROR_NOT_RESUMABLE;
    }

    size = mResponseHead->TotalEntitySize();
    Unused << mResponseHead->GetHeader(nsHttp::Last_Modified, lastmod);
    Unused << mResponseHead->GetHeader(nsHttp::ETag, etag);
  }
  nsCString entityID;
  NS_EscapeURL(etag.BeginReading(), etag.Length(),
               esc_AlwaysCopy | esc_FileBaseName | esc_Forced, entityID);
  entityID.Append('/');
  entityID.AppendInt(int64_t(size));
  entityID.Append('/');
  entityID.Append(lastmod);
  // NOTE: Appending lastmod as the last part avoids having to escape it

  aEntityID = entityID;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIConsoleReportCollector
//-----------------------------------------------------------------------------

void HttpBaseChannel::AddConsoleReport(
    uint32_t aErrorFlags, const nsACString& aCategory,
    nsContentUtils::PropertiesFile aPropertiesFile,
    const nsACString& aSourceFileURI, uint32_t aLineNumber,
    uint32_t aColumnNumber, const nsACString& aMessageName,
    const nsTArray<nsString>& aStringParams) {
  mReportCollector->AddConsoleReport(aErrorFlags, aCategory, aPropertiesFile,
                                     aSourceFileURI, aLineNumber, aColumnNumber,
                                     aMessageName, aStringParams);

  // If this channel is already part of a loadGroup, we can flush this console
  // report immediately.
  HttpBaseChannel::MaybeFlushConsoleReports();
}

void HttpBaseChannel::FlushReportsToConsole(uint64_t aInnerWindowID,
                                            ReportAction aAction) {
  mReportCollector->FlushReportsToConsole(aInnerWindowID, aAction);
}

void HttpBaseChannel::FlushReportsToConsoleForServiceWorkerScope(
    const nsACString& aScope, ReportAction aAction) {
  mReportCollector->FlushReportsToConsoleForServiceWorkerScope(aScope, aAction);
}

void HttpBaseChannel::FlushConsoleReports(dom::Document* aDocument,
                                          ReportAction aAction) {
  mReportCollector->FlushConsoleReports(aDocument, aAction);
}

void HttpBaseChannel::FlushConsoleReports(nsILoadGroup* aLoadGroup,
                                          ReportAction aAction) {
  mReportCollector->FlushConsoleReports(aLoadGroup, aAction);
}

void HttpBaseChannel::FlushConsoleReports(
    nsIConsoleReportCollector* aCollector) {
  mReportCollector->FlushConsoleReports(aCollector);
}

void HttpBaseChannel::StealConsoleReports(
    nsTArray<net::ConsoleReportCollected>& aReports) {
  mReportCollector->StealConsoleReports(aReports);
}

void HttpBaseChannel::ClearConsoleReports() {
  mReportCollector->ClearConsoleReports();
}

bool HttpBaseChannel::IsNavigation() {
  return LoadForceMainDocumentChannel() || (mLoadFlags & LOAD_DOCUMENT_URI);
}

bool HttpBaseChannel::BypassServiceWorker() const {
  return mLoadFlags & LOAD_BYPASS_SERVICE_WORKER;
}

bool HttpBaseChannel::ShouldIntercept(nsIURI* aURI) {
  nsCOMPtr<nsINetworkInterceptController> controller;
  GetCallback(controller);
  bool shouldIntercept = false;

  // We should never intercept internal redirects.  The ServiceWorker code
  // can trigger interntal redirects as the result of a FetchEvent.  If
  // we re-intercept then an infinite loop can occur.
  //
  // Its also important that we do not set the LOAD_BYPASS_SERVICE_WORKER
  // flag because an internal redirect occurs.  Its possible that another
  // interception should occur after the internal redirect.  For example,
  // if the ServiceWorker chooses not to call respondWith() the channel
  // will be reset with an internal redirect.  If the request is a navigation
  // and the network then triggers a redirect its possible the new URL
  // should be intercepted again.
  //
  // Note, HSTS upgrade redirects are often treated the same as internal
  // redirects.  In this case, however, we intentionally allow interception
  // of HSTS upgrade redirects.  This matches the expected spec behavior and
  // does not run the risk of infinite loops as described above.
  bool internalRedirect =
      mLastRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL;

  if (controller && mLoadInfo && !BypassServiceWorker() && !internalRedirect) {
    nsresult rv = controller->ShouldPrepareForIntercept(
        aURI ? aURI : mURI.get(), this, &shouldIntercept);
    if (NS_FAILED(rv)) {
      return false;
    }
  }
  return shouldIntercept;
}

void HttpBaseChannel::AddAsNonTailRequest() {
  MOZ_ASSERT(NS_IsMainThread());

  if (EnsureRequestContext()) {
    LOG((
        "HttpBaseChannel::AddAsNonTailRequest this=%p, rc=%p, already added=%d",
        this, mRequestContext.get(), (bool)LoadAddedAsNonTailRequest()));

    if (!LoadAddedAsNonTailRequest()) {
      mRequestContext->AddNonTailRequest();
      StoreAddedAsNonTailRequest(true);
    }
  }
}

void HttpBaseChannel::RemoveAsNonTailRequest() {
  MOZ_ASSERT(NS_IsMainThread());

  if (mRequestContext) {
    LOG(
        ("HttpBaseChannel::RemoveAsNonTailRequest this=%p, rc=%p, already "
         "added=%d",
         this, mRequestContext.get(), (bool)LoadAddedAsNonTailRequest()));

    if (LoadAddedAsNonTailRequest()) {
      mRequestContext->RemoveNonTailRequest();
      StoreAddedAsNonTailRequest(false);
    }
  }
}

#ifdef DEBUG
void HttpBaseChannel::AssertPrivateBrowsingId() {
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(this, loadContext);

  if (!loadContext) {
    return;
  }

  // We skip testing of favicon loading here since it could be triggered by XUL
  // image which uses SystemPrincipal. The SystemPrincpal doesn't have
  // mPrivateBrowsingId.
  if (mLoadInfo->GetLoadingPrincipal() &&
      mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal() &&
      mLoadInfo->InternalContentPolicyType() ==
          nsIContentPolicy::TYPE_INTERNAL_IMAGE_FAVICON) {
    return;
  }

  OriginAttributes docShellAttrs;
  loadContext->GetOriginAttributes(docShellAttrs);
  MOZ_ASSERT(mLoadInfo->GetOriginAttributes().mPrivateBrowsingId ==
                 docShellAttrs.mPrivateBrowsingId,
             "PrivateBrowsingId values are not the same between LoadInfo and "
             "LoadContext.");
}
#endif

already_AddRefed<nsILoadInfo> HttpBaseChannel::CloneLoadInfoForRedirect(
    nsIURI* aNewURI, uint32_t aRedirectFlags) {
  // make a copy of the loadinfo, append to the redirectchain
  // this will be set on the newly created channel for the redirect target.
  nsCOMPtr<nsILoadInfo> newLoadInfo =
      static_cast<mozilla::net::LoadInfo*>(mLoadInfo.get())->Clone();

  ExtContentPolicyType contentPolicyType =
      mLoadInfo->GetExternalContentPolicyType();
  if (contentPolicyType == ExtContentPolicy::TYPE_DOCUMENT ||
      contentPolicyType == ExtContentPolicy::TYPE_SUBDOCUMENT) {
    // Reset PrincipalToInherit to a null principal. We'll credit the the
    // redirecting resource's result principal as the new principal's precursor.
    // This means that a data: URI will end up loading in a process based on the
    // redirected-from URI.
    nsCOMPtr<nsIPrincipal> redirectPrincipal;
    nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(
        this, getter_AddRefs(redirectPrincipal));
    nsCOMPtr<nsIPrincipal> nullPrincipalToInherit =
        NullPrincipal::CreateWithInheritedAttributes(redirectPrincipal);
    newLoadInfo->SetPrincipalToInherit(nullPrincipalToInherit);
  }

  bool isTopLevelDoc = newLoadInfo->GetExternalContentPolicyType() ==
                       ExtContentPolicy::TYPE_DOCUMENT;

  if (isTopLevelDoc) {
    // re-compute the origin attributes of the loadInfo if it's top-level load.
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(this, loadContext);
    OriginAttributes docShellAttrs;
    if (loadContext) {
      loadContext->GetOriginAttributes(docShellAttrs);
    }

    OriginAttributes attrs = newLoadInfo->GetOriginAttributes();

    MOZ_ASSERT(
        docShellAttrs.mUserContextId == attrs.mUserContextId,
        "docshell and necko should have the same userContextId attribute.");
    MOZ_ASSERT(
        docShellAttrs.mPrivateBrowsingId == attrs.mPrivateBrowsingId,
        "docshell and necko should have the same privateBrowsingId attribute.");
    MOZ_ASSERT(docShellAttrs.mGeckoViewSessionContextId ==
                   attrs.mGeckoViewSessionContextId,
               "docshell and necko should have the same "
               "geckoViewSessionContextId attribute");

    attrs = docShellAttrs;
    attrs.SetFirstPartyDomain(true, aNewURI);
    newLoadInfo->SetOriginAttributes(attrs);

    // re-compute the upgrade insecure requests bit for document navigations
    // since it should only apply to same-origin navigations (redirects).
    // we only do this if the CSP of the triggering element (the cspToInherit)
    // uses 'upgrade-insecure-requests', otherwise UIR does not apply.
    nsCOMPtr<nsIContentSecurityPolicy> csp = newLoadInfo->GetCspToInherit();
    if (csp) {
      bool upgradeInsecureRequests = false;
      csp->GetUpgradeInsecureRequests(&upgradeInsecureRequests);
      if (upgradeInsecureRequests) {
        nsCOMPtr<nsIPrincipal> resultPrincipal =
            BasePrincipal::CreateContentPrincipal(
                aNewURI, newLoadInfo->GetOriginAttributes());
        bool isConsideredSameOriginforUIR =
            nsContentSecurityUtils::IsConsideredSameOriginForUIR(
                newLoadInfo->TriggeringPrincipal(), resultPrincipal);
        static_cast<mozilla::net::LoadInfo*>(newLoadInfo.get())
            ->SetUpgradeInsecureRequests(isConsideredSameOriginforUIR);
      }
    }
  }

  // Leave empty, we want a 'clean ground' when creating the new channel.
  // This will be ensured to be either set by the protocol handler or set
  // to the redirect target URI properly after the channel creation.
  newLoadInfo->SetResultPrincipalURI(nullptr);

  bool isInternalRedirect =
      (aRedirectFlags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                         nsIChannelEventSink::REDIRECT_STS_UPGRADE));

  // Reset our sandboxed null principal ID when cloning loadInfo for an
  // externally visible redirect.
  if (!isInternalRedirect) {
    // If we've redirected from http to something that isn't, clear
    // the "external" flag, as loads that now go to other apps should be
    // allowed to go ahead and not trip infinite-loop protection
    // (see bug 1717314 for context).
    if (!aNewURI->SchemeIs("http") && !aNewURI->SchemeIs("https")) {
      newLoadInfo->SetLoadTriggeredFromExternal(false);
    }
    newLoadInfo->ResetSandboxedNullPrincipalID();
  }

  newLoadInfo->AppendRedirectHistoryEntry(this, isInternalRedirect);

  return newLoadInfo.forget();
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetNewListener(nsIStreamListener* aListener,
                                bool aMustApplyContentConversion,
                                nsIStreamListener** _retval) {
  LOG((
      "HttpBaseChannel::SetNewListener [this=%p, mListener=%p, newListener=%p]",
      this, mListener.get(), aListener));

  if (!LoadTracingEnabled()) return NS_ERROR_FAILURE;

  NS_ENSURE_STATE(mListener);
  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIStreamListener> wrapper = new nsStreamListenerWrapper(mListener);

  wrapper.forget(_retval);
  mListener = aListener;
  if (aMustApplyContentConversion) {
    StoreListenerRequiresContentConversion(true);
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel helpers
//-----------------------------------------------------------------------------

void HttpBaseChannel::ReleaseListeners() {
  MOZ_ASSERT(mCurrentThread->IsOnCurrentThread(),
             "Should only be called on the current thread");

  mListener = nullptr;
  mCallbacks = nullptr;
  mProgressSink = nullptr;
  mCompressListener = nullptr;
}

void HttpBaseChannel::DoNotifyListener() {
  LOG(("HttpBaseChannel::DoNotifyListener this=%p", this));

  // In case nsHttpChannel::OnStartRequest wasn't called (e.g. due to flag
  // LOAD_ONLY_IF_MODIFIED) we want to set AfterOnStartRequestBegun to true
  // before notifying listener.
  if (!LoadAfterOnStartRequestBegun()) {
    StoreAfterOnStartRequestBegun(true);
  }

  if (mListener && !LoadOnStartRequestCalled()) {
    nsCOMPtr<nsIStreamListener> listener = mListener;
    StoreOnStartRequestCalled(true);
    listener->OnStartRequest(this);
  }
  StoreOnStartRequestCalled(true);

  // Make sure IsPending is set to false. At this moment we are done from
  // the point of view of our consumer and we have to report our self
  // as not-pending.
  StoreIsPending(false);

  if (mListener && !LoadOnStopRequestCalled()) {
    nsCOMPtr<nsIStreamListener> listener = mListener;
    StoreOnStopRequestCalled(true);
    listener->OnStopRequest(this, mStatus);
  }
  StoreOnStopRequestCalled(true);

  // notify "http-on-stop-connect" observers
  gHttpHandler->OnStopRequest(this);

  // This channel has finished its job, potentially release any tail-blocked
  // requests with this.
  RemoveAsNonTailRequest();

  // We have to make sure to drop the references to listeners and callbacks
  // no longer needed.
  ReleaseListeners();

  DoNotifyListenerCleanup();

  // If this is a navigation, then we must let the docshell flush the reports
  // to the console later.  The LoadDocument() is pointing at the detached
  // document that started the navigation.  We want to show the reports on the
  // new document.  Otherwise the console is wiped and the user never sees
  // the information.
  if (!IsNavigation()) {
    if (mLoadGroup) {
      FlushConsoleReports(mLoadGroup);
    } else {
      RefPtr<dom::Document> doc;
      mLoadInfo->GetLoadingDocument(getter_AddRefs(doc));
      FlushConsoleReports(doc);
    }
  }
}

void HttpBaseChannel::AddCookiesToRequest() {
  if (mLoadFlags & LOAD_ANONYMOUS) {
    return;
  }

  bool useCookieService = (XRE_IsParentProcess());
  nsAutoCString cookie;
  if (useCookieService) {
    nsICookieService* cs = gHttpHandler->GetCookieService();
    if (cs) {
      cs->GetCookieStringFromHttp(mURI, this, cookie);
    }

    if (cookie.IsEmpty()) {
      cookie = mUserSetCookieHeader;
    } else if (!mUserSetCookieHeader.IsEmpty()) {
      cookie.AppendLiteral("; ");
      cookie.Append(mUserSetCookieHeader);
    }
  } else {
    cookie = mUserSetCookieHeader;
  }

  // If we are in the child process, we want the parent seeing any
  // cookie headers that might have been set by SetRequestHeader()
  SetRequestHeader(nsDependentCString(nsHttp::Cookie), cookie, false);
}

/* static */
void HttpBaseChannel::PropagateReferenceIfNeeded(
    nsIURI* aURI, nsCOMPtr<nsIURI>& aRedirectURI) {
  bool hasRef = false;
  nsresult rv = aRedirectURI->GetHasRef(&hasRef);
  if (NS_SUCCEEDED(rv) && !hasRef) {
    nsAutoCString ref;
    aURI->GetRef(ref);
    if (!ref.IsEmpty()) {
      // NOTE: SetRef will fail if mRedirectURI is immutable
      // (e.g. an about: URI)... Oh well.
      Unused << NS_MutateURI(aRedirectURI).SetRef(ref).Finalize(aRedirectURI);
    }
  }
}

bool HttpBaseChannel::ShouldRewriteRedirectToGET(
    uint32_t httpStatus, nsHttpRequestHead::ParsedMethodType method) {
  // for 301 and 302, only rewrite POST
  if (httpStatus == 301 || httpStatus == 302) {
    return method == nsHttpRequestHead::kMethod_Post;
  }

  // rewrite for 303 unless it was HEAD
  if (httpStatus == 303) return method != nsHttpRequestHead::kMethod_Head;

  // otherwise, such as for 307, do not rewrite
  return false;
}

NS_IMETHODIMP
HttpBaseChannel::ShouldStripRequestBodyHeader(const nsACString& aMethod,
                                              bool* aResult) {
  *aResult = false;
  uint32_t httpStatus = 0;
  if (NS_FAILED(GetResponseStatus(&httpStatus))) {
    return NS_OK;
  }

  nsAutoCString method(aMethod);
  nsHttpRequestHead::ParsedMethodType parsedMethod;
  nsHttpRequestHead::ParseMethod(method, parsedMethod);
  // Fetch 4.4.11, which is slightly different than the perserved method
  // algrorithm: strip request-body-header for GET->GET redirection for 303.
  *aResult =
      ShouldRewriteRedirectToGET(httpStatus, parsedMethod) &&
      !(httpStatus == 303 && parsedMethod == nsHttpRequestHead::kMethod_Get);

  return NS_OK;
}

HttpBaseChannel::ReplacementChannelConfig
HttpBaseChannel::CloneReplacementChannelConfig(bool aPreserveMethod,
                                               uint32_t aRedirectFlags,
                                               ReplacementReason aReason) {
  ReplacementChannelConfig config;
  config.redirectFlags = aRedirectFlags;
  config.classOfService = mClassOfService;

  if (mPrivateBrowsingOverriden) {
    config.privateBrowsing = Some(mPrivateBrowsing);
  }

  if (mReferrerInfo) {
    // When cloning for a document channel replacement (parent process
    // copying values for a new content process channel), this happens after
    // OnStartRequest so we have the headers for the response available.
    // We don't want to apply them to the referrer for the channel though,
    // since that is the referrer for the current document, and the header
    // should only apply to navigations from the current document.
    if (aReason == ReplacementReason::DocumentChannel) {
      config.referrerInfo = mReferrerInfo;
    } else {
      dom::ReferrerPolicy referrerPolicy = dom::ReferrerPolicy::_empty;
      nsAutoCString tRPHeaderCValue;
      Unused << GetResponseHeader("referrer-policy"_ns, tRPHeaderCValue);
      NS_ConvertUTF8toUTF16 tRPHeaderValue(tRPHeaderCValue);

      if (!tRPHeaderValue.IsEmpty()) {
        referrerPolicy =
            dom::ReferrerInfo::ReferrerPolicyFromHeaderString(tRPHeaderValue);
      }

      if (referrerPolicy != dom::ReferrerPolicy::_empty) {
        // We may reuse computed referrer in redirect, so if referrerPolicy
        // changes, we must not use the old computed value, and have to compute
        // again.
        nsCOMPtr<nsIReferrerInfo> referrerInfo =
            dom::ReferrerInfo::CreateFromOtherAndPolicyOverride(mReferrerInfo,
                                                                referrerPolicy);
        config.referrerInfo = referrerInfo;
      } else {
        config.referrerInfo = mReferrerInfo;
      }
    }
  }

  nsCOMPtr<nsITimedChannel> oldTimedChannel(
      do_QueryInterface(static_cast<nsIHttpChannel*>(this)));
  if (oldTimedChannel) {
    config.timedChannelInfo = Some(dom::TimedChannelInfo());
    config.timedChannelInfo->timingEnabled() = LoadTimingEnabled();
    config.timedChannelInfo->redirectCount() = mRedirectCount;
    config.timedChannelInfo->internalRedirectCount() = mInternalRedirectCount;
    config.timedChannelInfo->asyncOpen() = mAsyncOpenTime;
    config.timedChannelInfo->channelCreation() = mChannelCreationTimestamp;
    config.timedChannelInfo->redirectStart() = mRedirectStartTimeStamp;
    config.timedChannelInfo->redirectEnd() = mRedirectEndTimeStamp;
    config.timedChannelInfo->initiatorType() = mInitiatorType;
    config.timedChannelInfo->allRedirectsSameOrigin() =
        LoadAllRedirectsSameOrigin();
    config.timedChannelInfo->allRedirectsPassTimingAllowCheck() =
        LoadAllRedirectsPassTimingAllowCheck();
    // Execute the timing allow check to determine whether
    // to report the redirect timing info
    nsCOMPtr<nsILoadInfo> loadInfo = LoadInfo();
    // TYPE_DOCUMENT loads don't have a loadingPrincipal, so we can't set
    // AllRedirectsPassTimingAllowCheck on them.
    if (loadInfo->GetExternalContentPolicyType() !=
        ExtContentPolicy::TYPE_DOCUMENT) {
      nsCOMPtr<nsIPrincipal> principal = loadInfo->GetLoadingPrincipal();
      config.timedChannelInfo->timingAllowCheckForPrincipal() =
          Some(oldTimedChannel->TimingAllowCheck(principal));
    }

    config.timedChannelInfo->allRedirectsPassTimingAllowCheck() =
        LoadAllRedirectsPassTimingAllowCheck();
    config.timedChannelInfo->launchServiceWorkerStart() =
        mLaunchServiceWorkerStart;
    config.timedChannelInfo->launchServiceWorkerEnd() = mLaunchServiceWorkerEnd;
    config.timedChannelInfo->dispatchFetchEventStart() =
        mDispatchFetchEventStart;
    config.timedChannelInfo->dispatchFetchEventEnd() = mDispatchFetchEventEnd;
    config.timedChannelInfo->handleFetchEventStart() = mHandleFetchEventStart;
    config.timedChannelInfo->handleFetchEventEnd() = mHandleFetchEventEnd;
    config.timedChannelInfo->responseStart() =
        mTransactionTimings.responseStart;
    config.timedChannelInfo->responseEnd() = mTransactionTimings.responseEnd;
  }

  if (aPreserveMethod) {
    // since preserveMethod is true, we need to ensure that the appropriate
    // request method gets set on the channel, regardless of whether or not
    // we set the upload stream above. This means SetRequestMethod() will
    // be called twice if ExplicitSetUploadStream() gets called above.

    nsAutoCString method;
    mRequestHead.Method(method);
    config.method = Some(method);

    if (mUploadStream) {
      // rewind upload stream
      nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
      if (seekable) {
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);
      }
      config.uploadStream = mUploadStream;
    }
    config.uploadStreamLength = mReqContentLength;
    config.uploadStreamHasHeaders = LoadUploadStreamHasHeaders();

    nsAutoCString contentType;
    nsresult rv = mRequestHead.GetHeader(nsHttp::Content_Type, contentType);
    if (NS_SUCCEEDED(rv)) {
      config.contentType = Some(contentType);
    }

    nsAutoCString contentLength;
    rv = mRequestHead.GetHeader(nsHttp::Content_Length, contentLength);
    if (NS_SUCCEEDED(rv)) {
      config.contentLength = Some(contentLength);
    }
  }

  return config;
}

/* static */ void HttpBaseChannel::ConfigureReplacementChannel(
    nsIChannel* newChannel, const ReplacementChannelConfig& config,
    ReplacementReason aReason) {
  nsCOMPtr<nsIClassOfService> cos(do_QueryInterface(newChannel));
  if (cos) {
    cos->SetClassFlags(config.classOfService);
  }

  // Try to preserve the privacy bit if it has been overridden
  if (config.privateBrowsing) {
    nsCOMPtr<nsIPrivateBrowsingChannel> newPBChannel =
        do_QueryInterface(newChannel);
    if (newPBChannel) {
      newPBChannel->SetPrivate(*config.privateBrowsing);
    }
  }

  // Transfer the timing data (if we are dealing with an nsITimedChannel).
  nsCOMPtr<nsITimedChannel> newTimedChannel(do_QueryInterface(newChannel));
  if (config.timedChannelInfo && newTimedChannel) {
    newTimedChannel->SetTimingEnabled(config.timedChannelInfo->timingEnabled());

    // If we're an internal redirect, or a document channel replacement,
    // then we shouldn't record any new timing for this and just copy
    // over the existing values.
    bool shouldHideTiming = aReason != ReplacementReason::Redirect;
    if (shouldHideTiming) {
      newTimedChannel->SetRedirectCount(
          config.timedChannelInfo->redirectCount());
      int32_t newCount = config.timedChannelInfo->internalRedirectCount() + 1;
      newTimedChannel->SetInternalRedirectCount(std::max(
          newCount, static_cast<int32_t>(
                        config.timedChannelInfo->internalRedirectCount())));
    } else {
      int32_t newCount = config.timedChannelInfo->redirectCount() + 1;
      newTimedChannel->SetRedirectCount(std::max(
          newCount,
          static_cast<int32_t>(config.timedChannelInfo->redirectCount())));
      newTimedChannel->SetInternalRedirectCount(
          config.timedChannelInfo->internalRedirectCount());
    }

    if (shouldHideTiming) {
      if (!config.timedChannelInfo->channelCreation().IsNull()) {
        newTimedChannel->SetChannelCreation(
            config.timedChannelInfo->channelCreation());
      }

      if (!config.timedChannelInfo->asyncOpen().IsNull()) {
        newTimedChannel->SetAsyncOpen(config.timedChannelInfo->asyncOpen());
      }
    }

    // If the RedirectStart is null, we will use the AsyncOpen value of the
    // previous channel (this is the first redirect in the redirects chain).
    if (config.timedChannelInfo->redirectStart().IsNull()) {
      // Only do this for real redirects.  Internal redirects should be hidden.
      if (!shouldHideTiming) {
        newTimedChannel->SetRedirectStart(config.timedChannelInfo->asyncOpen());
      }
    } else {
      newTimedChannel->SetRedirectStart(
          config.timedChannelInfo->redirectStart());
    }

    // For internal redirects just propagate the last redirect end time
    // forward.  Otherwise the new redirect end time is the last response
    // end time.
    TimeStamp newRedirectEnd;
    if (shouldHideTiming) {
      newRedirectEnd = config.timedChannelInfo->redirectEnd();
    } else {
      newRedirectEnd = config.timedChannelInfo->responseEnd();
    }
    newTimedChannel->SetRedirectEnd(newRedirectEnd);

    newTimedChannel->SetInitiatorType(config.timedChannelInfo->initiatorType());

    nsCOMPtr<nsILoadInfo> loadInfo = newChannel->LoadInfo();
    MOZ_ASSERT(loadInfo);

    newTimedChannel->SetAllRedirectsSameOrigin(
        config.timedChannelInfo->allRedirectsSameOrigin());

    if (config.timedChannelInfo->timingAllowCheckForPrincipal()) {
      newTimedChannel->SetAllRedirectsPassTimingAllowCheck(
          config.timedChannelInfo->allRedirectsPassTimingAllowCheck() &&
          *config.timedChannelInfo->timingAllowCheckForPrincipal());
    }

    // Propagate service worker measurements across redirects.  The
    // PeformanceResourceTiming.workerStart API expects to see the
    // worker start time after a redirect.
    newTimedChannel->SetLaunchServiceWorkerStart(
        config.timedChannelInfo->launchServiceWorkerStart());
    newTimedChannel->SetLaunchServiceWorkerEnd(
        config.timedChannelInfo->launchServiceWorkerEnd());
    newTimedChannel->SetDispatchFetchEventStart(
        config.timedChannelInfo->dispatchFetchEventStart());
    newTimedChannel->SetDispatchFetchEventEnd(
        config.timedChannelInfo->dispatchFetchEventEnd());
    newTimedChannel->SetHandleFetchEventStart(
        config.timedChannelInfo->handleFetchEventStart());
    newTimedChannel->SetHandleFetchEventEnd(
        config.timedChannelInfo->handleFetchEventEnd());
  }

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
  if (!httpChannel) {
    return;  // no other options to set
  }

  if (config.uploadStream) {
    nsCOMPtr<nsIUploadChannel> uploadChannel = do_QueryInterface(httpChannel);
    nsCOMPtr<nsIUploadChannel2> uploadChannel2 = do_QueryInterface(httpChannel);
    if (uploadChannel2 || uploadChannel) {
      // replicate original call to SetUploadStream...
      if (uploadChannel2) {
        const nsACString& ctype =
            config.contentType ? *config.contentType : VoidCString();
        // If header is not present mRequestHead.HasHeaderValue will truncated
        // it.  But we want to end up with a void string, not an empty string,
        // because ExplicitSetUploadStream treats the former as "no header" and
        // the latter as "header with empty string value".

        const nsACString& method =
            config.method ? *config.method : VoidCString();

        uploadChannel2->ExplicitSetUploadStream(
            config.uploadStream, ctype, config.uploadStreamLength, method,
            config.uploadStreamHasHeaders);
      } else {
        if (config.uploadStreamHasHeaders) {
          uploadChannel->SetUploadStream(config.uploadStream, ""_ns,
                                         config.uploadStreamLength);
        } else {
          nsAutoCString ctype;
          if (config.contentType) {
            ctype = *config.contentType;
          } else {
            ctype = "application/octet-stream"_ns;
          }
          if (config.contentLength && !config.contentLength->IsEmpty()) {
            uploadChannel->SetUploadStream(
                config.uploadStream, ctype,
                nsCRT::atoll(config.contentLength->get()));
          }
        }
      }
    }
  }

  if (config.referrerInfo) {
    DebugOnly<nsresult> success{};
    success = httpChannel->SetReferrerInfo(config.referrerInfo);
    MOZ_ASSERT(NS_SUCCEEDED(success));
  }

  if (config.method) {
    DebugOnly<nsresult> rv = httpChannel->SetRequestMethod(*config.method);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }
}

HttpBaseChannel::ReplacementChannelConfig::ReplacementChannelConfig(
    const dom::ReplacementChannelConfigInit& aInit) {
  redirectFlags = aInit.redirectFlags();
  classOfService = aInit.classOfService();
  privateBrowsing = aInit.privateBrowsing();
  method = aInit.method();
  referrerInfo = aInit.referrerInfo();
  timedChannelInfo = aInit.timedChannelInfo();
  if (RemoteLazyInputStreamChild* actor =
          static_cast<RemoteLazyInputStreamChild*>(aInit.uploadStreamChild())) {
    uploadStreamLength = actor->Size();
    uploadStream = actor->CreateStream();
    // actor can be deleted by CreateStream, so don't touch it
    // after this.
  } else {
    uploadStreamLength = 0;
  }
  uploadStreamHasHeaders = aInit.uploadStreamHasHeaders();
  contentType = aInit.contentType();
  contentLength = aInit.contentLength();
}

dom::ReplacementChannelConfigInit
HttpBaseChannel::ReplacementChannelConfig::Serialize(
    dom::ContentParent* aParent) {
  dom::ReplacementChannelConfigInit config;
  config.redirectFlags() = redirectFlags;
  config.classOfService() = classOfService;
  config.privateBrowsing() = privateBrowsing;
  config.method() = method;
  config.referrerInfo() = referrerInfo;
  config.timedChannelInfo() = timedChannelInfo;
  if (uploadStream) {
    RemoteLazyStream ipdlStream;
    RemoteLazyInputStreamUtils::SerializeInputStream(
        uploadStream, uploadStreamLength, ipdlStream, aParent);
    config.uploadStreamParent() = ipdlStream;
  }
  config.uploadStreamHasHeaders() = uploadStreamHasHeaders;
  config.contentType() = contentType;
  config.contentLength() = contentLength;

  return config;
}

nsresult HttpBaseChannel::SetupReplacementChannel(nsIURI* newURI,
                                                  nsIChannel* newChannel,
                                                  bool preserveMethod,
                                                  uint32_t redirectFlags) {
  nsresult rv;

  LOG(
      ("HttpBaseChannel::SetupReplacementChannel "
       "[this=%p newChannel=%p preserveMethod=%d]",
       this, newChannel, preserveMethod));

  // Ensure the channel's loadInfo's result principal URI so that it's
  // either non-null or updated to the redirect target URI.
  // We must do this because in case the loadInfo's result principal URI
  // is null, it would be taken from OriginalURI of the channel.  But we
  // overwrite it with the whole redirect chain first URI before opening
  // the target channel, hence the information would be lost.
  // If the protocol handler that created the channel wants to use
  // the originalURI of the channel as the principal URI, this fulfills
  // that request - newURI is the original URI of the channel.
  nsCOMPtr<nsILoadInfo> newLoadInfo = newChannel->LoadInfo();
  nsCOMPtr<nsIURI> resultPrincipalURI;
  rv = newLoadInfo->GetResultPrincipalURI(getter_AddRefs(resultPrincipalURI));
  NS_ENSURE_SUCCESS(rv, rv);
  if (!resultPrincipalURI) {
    rv = newLoadInfo->SetResultPrincipalURI(newURI);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsLoadFlags loadFlags = mLoadFlags;
  loadFlags |= LOAD_REPLACE;

  // if the original channel was using SSL and this channel is not using
  // SSL, then no need to inhibit persistent caching.  however, if the
  // original channel was not using SSL and has INHIBIT_PERSISTENT_CACHING
  // set, then allow the flag to apply to the redirected channel as well.
  // since we force set INHIBIT_PERSISTENT_CACHING on all HTTPS channels,
  // we only need to check if the original channel was using SSL.
  if (mURI->SchemeIs("https")) {
    loadFlags &= ~INHIBIT_PERSISTENT_CACHING;
  }

  newChannel->SetLoadFlags(loadFlags);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);

  ReplacementReason redirectType =
      (redirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL)
          ? ReplacementReason::InternalRedirect
          : ReplacementReason::Redirect;
  ReplacementChannelConfig config = CloneReplacementChannelConfig(
      preserveMethod, redirectFlags, redirectType);
  ConfigureReplacementChannel(newChannel, config, redirectType);

  // Check whether or not this was a cross-domain redirect.
  nsCOMPtr<nsITimedChannel> newTimedChannel(do_QueryInterface(newChannel));
  bool sameOriginWithOriginalUri = SameOriginWithOriginalUri(newURI);
  if (config.timedChannelInfo && newTimedChannel) {
    newTimedChannel->SetAllRedirectsSameOrigin(
        config.timedChannelInfo->allRedirectsSameOrigin() &&
        sameOriginWithOriginalUri);
  }

  newChannel->SetLoadGroup(mLoadGroup);
  newChannel->SetNotificationCallbacks(mCallbacks);
  // TODO: create tests for cross-origin redirect in bug 1662896.
  if (sameOriginWithOriginalUri) {
    newChannel->SetContentDisposition(mContentDispositionHint);
    if (mContentDispositionFilename) {
      newChannel->SetContentDispositionFilename(*mContentDispositionFilename);
    }
  }

  if (!httpChannel) return NS_OK;  // no other options to set

  // Preserve the CORS preflight information.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(newChannel);
  if (httpInternal) {
    httpInternal->SetLastRedirectFlags(redirectFlags);

    if (LoadRequireCORSPreflight()) {
      httpInternal->SetCorsPreflightParameters(mUnsafeHeaders, false);
    }
  }

  // convey the LoadAllowSTS() flags
  rv = httpChannel->SetAllowSTS(LoadAllowSTS());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // convey the Accept header value
  {
    nsAutoCString oldAcceptValue;
    nsresult hasHeader = mRequestHead.GetHeader(nsHttp::Accept, oldAcceptValue);
    if (NS_SUCCEEDED(hasHeader)) {
      rv = httpChannel->SetRequestHeader("Accept"_ns, oldAcceptValue, false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  // convey the User-Agent header value
  // since we might be setting custom user agent from DevTools.
  if (httpInternal && mCorsMode == CORS_MODE_NO_CORS &&
      redirectType == ReplacementReason::Redirect) {
    nsAutoCString oldUserAgent;
    nsresult hasHeader =
        mRequestHead.GetHeader(nsHttp::User_Agent, oldUserAgent);
    if (NS_SUCCEEDED(hasHeader)) {
      rv = httpChannel->SetRequestHeader("User-Agent"_ns, oldUserAgent, false);
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }
  }

  // share the request context - see bug 1236650
  rv = httpChannel->SetRequestContextID(mRequestContextID);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // When on the parent process, the channel can't attempt to get it itself.
  // When on the child process, it would be waste to query it again.
  rv = httpChannel->SetTopBrowsingContextId(mTopBrowsingContextId);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Not setting this flag would break carrying permissions down to the child
  // process when the channel is artificially forced to be a main document load.
  rv = httpChannel->SetIsMainDocumentChannel(LoadForceMainDocumentChannel());
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Preserve the loading order
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(newChannel);
  if (p) {
    p->SetPriority(mPriority);
  }

  if (httpInternal) {
    // Convey third party cookie, conservative, and spdy flags.
    rv = httpInternal->SetThirdPartyFlags(LoadThirdPartyFlags());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetAllowSpdy(LoadAllowSpdy());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetAllowHttp3(LoadAllowHttp3());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetAllowAltSvc(LoadAllowAltSvc());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetBeConservative(LoadBeConservative());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetIsTRRServiceChannel(LoadIsTRRServiceChannel());
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    rv = httpInternal->SetTlsFlags(mTlsFlags);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Ensure the type of realChannel involves all types it may redirect to.
    // Such as nsHttpChannel and InterceptedChannel.
    // Even thought InterceptedChannel itself doesn't require these information,
    // it may still be necessary for the following redirections.
    // E.g. nsHttpChannel -> InterceptedChannel -> nsHttpChannel
    RefPtr<HttpBaseChannel> realChannel;
    CallQueryInterface(newChannel, realChannel.StartAssignment());
    if (realChannel) {
      realChannel->SetTopWindowURI(mTopWindowURI);
    }

    // update the DocumentURI indicator since we are being redirected.
    // if this was a top-level document channel, then the new channel
    // should have its mDocumentURI point to newURI; otherwise, we
    // just need to pass along our mDocumentURI to the new channel.
    if (newURI && (mURI == mDocumentURI)) {
      rv = httpInternal->SetDocumentURI(newURI);
    } else {
      rv = httpInternal->SetDocumentURI(mDocumentURI);
    }
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // if there is a chain of keys for redirect-responses we transfer it to
    // the new channel (see bug #561276)
    if (mRedirectedCachekeys) {
      LOG(
          ("HttpBaseChannel::SetupReplacementChannel "
           "[this=%p] transferring chain of redirect cache-keys",
           this));
      rv = httpInternal->SetCacheKeysRedirectChain(
          mRedirectedCachekeys.release());
      MOZ_ASSERT(NS_SUCCEEDED(rv));
    }

    // Preserve CORS mode flag.
    rv = httpInternal->SetCorsMode(mCorsMode);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Preserve Redirect mode flag.
    rv = httpInternal->SetRedirectMode(mRedirectMode);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    // Preserve Integrity metadata.
    rv = httpInternal->SetIntegrityMetadata(mIntegrityMetadata);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    httpInternal->SetAltDataForChild(LoadAltDataForChild());
    if (LoadDisableAltDataCache()) {
      httpInternal->DisableAltDataCache();
    }
  }

  // transfer any properties
  nsCOMPtr<nsIWritablePropertyBag> bag(do_QueryInterface(newChannel));
  if (bag) {
    for (const auto& entry : mPropertyHash) {
      bag->SetProperty(entry.GetKey(), entry.GetWeak());
    }
  }

  // Pass the preferred alt-data type on to the new channel.
  nsCOMPtr<nsICacheInfoChannel> cacheInfoChan(do_QueryInterface(newChannel));
  if (cacheInfoChan) {
    for (auto& data : mPreferredCachedAltDataTypes) {
      cacheInfoChan->PreferAlternativeDataType(data.type(), data.contentType(),
                                               data.deliverAltData());
    }

    if (LoadForceValidateCacheContent()) {
      Unused << cacheInfoChan->SetForceValidateCacheContent(true);
    }
  }

  if (redirectFlags & (nsIChannelEventSink::REDIRECT_INTERNAL |
                       nsIChannelEventSink::REDIRECT_STS_UPGRADE)) {
    // Copy non-origin related headers to the new channel.
    nsCOMPtr<nsIHttpHeaderVisitor> visitor =
        new AddHeadersToChannelVisitor(httpChannel);
    rv = mRequestHead.VisitHeaders(visitor);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // This channel has been redirected. Don't report timing info.
  StoreTimingEnabled(false);
  return NS_OK;
}

// Redirect Tracking
bool HttpBaseChannel::SameOriginWithOriginalUri(nsIURI* aURI) {
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  bool isPrivateWin = mLoadInfo->GetOriginAttributes().mPrivateBrowsingId > 0;
  nsresult rv =
      ssm->CheckSameOriginURI(aURI, mOriginalURI, false, isPrivateWin);
  return (NS_SUCCEEDED(rv));
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIClassifiedChannel

NS_IMETHODIMP
HttpBaseChannel::GetMatchedList(nsACString& aList) {
  aList = mMatchedList;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetMatchedProvider(nsACString& aProvider) {
  aProvider = mMatchedProvider;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetMatchedFullHash(nsACString& aFullHash) {
  aFullHash = mMatchedFullHash;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetMatchedInfo(const nsACString& aList,
                                const nsACString& aProvider,
                                const nsACString& aFullHash) {
  NS_ENSURE_ARG(!aList.IsEmpty());

  mMatchedList = aList;
  mMatchedProvider = aProvider;
  mMatchedFullHash = aFullHash;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetMatchedTrackingLists(nsTArray<nsCString>& aLists) {
  aLists = mMatchedTrackingLists.Clone();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetMatchedTrackingFullHashes(
    nsTArray<nsCString>& aFullHashes) {
  aFullHashes = mMatchedTrackingFullHashes.Clone();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetMatchedTrackingInfo(
    const nsTArray<nsCString>& aLists, const nsTArray<nsCString>& aFullHashes) {
  NS_ENSURE_ARG(!aLists.IsEmpty());
  // aFullHashes can be empty for non hash-matching algorithm, for example,
  // host based test entries in preference.

  mMatchedTrackingLists = aLists.Clone();
  mMatchedTrackingFullHashes = aFullHashes.Clone();
  return NS_OK;
}
//-----------------------------------------------------------------------------
// HttpBaseChannel::nsITimedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetTimingEnabled(bool enabled) {
  StoreTimingEnabled(enabled);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTimingEnabled(bool* _retval) {
  *_retval = LoadTimingEnabled();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetChannelCreation(TimeStamp* _retval) {
  *_retval = mChannelCreationTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetChannelCreation(TimeStamp aValue) {
  MOZ_DIAGNOSTIC_ASSERT(!aValue.IsNull());
  TimeDuration adjust = aValue - mChannelCreationTimestamp;
  mChannelCreationTimestamp = aValue;
  mChannelCreationTime += (PRTime)adjust.ToMicroseconds();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAsyncOpen(TimeStamp* _retval) {
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAsyncOpen(TimeStamp aValue) {
  MOZ_DIAGNOSTIC_ASSERT(!aValue.IsNull());
  mAsyncOpenTime = aValue;
  StoreAsyncOpenTimeOverriden(true);
  return NS_OK;
}

/**
 * @return the number of redirects. There is no check for cross-domain
 * redirects. This check must be done by the consumers.
 */
NS_IMETHODIMP
HttpBaseChannel::GetRedirectCount(uint8_t* aRedirectCount) {
  *aRedirectCount = mRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectCount(uint8_t aRedirectCount) {
  mRedirectCount = aRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetInternalRedirectCount(uint8_t* aRedirectCount) {
  *aRedirectCount = mInternalRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetInternalRedirectCount(uint8_t aRedirectCount) {
  mInternalRedirectCount = aRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectStart(TimeStamp* _retval) {
  *_retval = mRedirectStartTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectStart(TimeStamp aRedirectStart) {
  mRedirectStartTimeStamp = aRedirectStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectEnd(TimeStamp* _retval) {
  *_retval = mRedirectEndTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectEnd(TimeStamp aRedirectEnd) {
  mRedirectEndTimeStamp = aRedirectEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllRedirectsSameOrigin(bool* aAllRedirectsSameOrigin) {
  *aAllRedirectsSameOrigin = LoadAllRedirectsSameOrigin();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllRedirectsSameOrigin(bool aAllRedirectsSameOrigin) {
  StoreAllRedirectsSameOrigin(aAllRedirectsSameOrigin);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllRedirectsPassTimingAllowCheck(bool* aPassesCheck) {
  *aPassesCheck = LoadAllRedirectsPassTimingAllowCheck();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllRedirectsPassTimingAllowCheck(bool aPassesCheck) {
  StoreAllRedirectsPassTimingAllowCheck(aPassesCheck);
  return NS_OK;
}

// http://www.w3.org/TR/resource-timing/#timing-allow-check
NS_IMETHODIMP
HttpBaseChannel::TimingAllowCheck(nsIPrincipal* aOrigin, bool* _retval) {
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> resourcePrincipal;
  nsresult rv =
      ssm->GetChannelURIPrincipal(this, getter_AddRefs(resourcePrincipal));
  if (NS_FAILED(rv) || !resourcePrincipal || !aOrigin) {
    *_retval = false;
    return NS_OK;
  }

  bool sameOrigin = false;
  rv = resourcePrincipal->Equals(aOrigin, &sameOrigin);
  if (NS_SUCCEEDED(rv) && sameOrigin) {
    *_retval = true;
    return NS_OK;
  }

  nsAutoCString headerValue;
  rv = GetResponseHeader("Timing-Allow-Origin"_ns, headerValue);
  if (NS_FAILED(rv)) {
    *_retval = false;
    return NS_OK;
  }

  nsAutoCString origin;
  aOrigin->GetAsciiOrigin(origin);

  Tokenizer p(headerValue);
  Tokenizer::Token t;

  p.Record();
  nsAutoCString headerItem;
  while (p.Next(t)) {
    if (t.Type() == Tokenizer::TOKEN_EOF ||
        t.Equals(Tokenizer::Token::Char(','))) {
      p.Claim(headerItem);
      nsHttp::TrimHTTPWhitespace(headerItem, headerItem);
      // If the list item contains a case-sensitive match for the value of the
      // origin, or a wildcard, return pass
      if (headerItem == origin || headerItem == "*") {
        *_retval = true;
        return NS_OK;
      }
      // We start recording again for the following items in the list
      p.Record();
    }
  }

  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLaunchServiceWorkerStart(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mLaunchServiceWorkerStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLaunchServiceWorkerStart(TimeStamp aTimeStamp) {
  mLaunchServiceWorkerStart = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLaunchServiceWorkerEnd(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mLaunchServiceWorkerEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLaunchServiceWorkerEnd(TimeStamp aTimeStamp) {
  mLaunchServiceWorkerEnd = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetDispatchFetchEventStart(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mDispatchFetchEventStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetDispatchFetchEventStart(TimeStamp aTimeStamp) {
  mDispatchFetchEventStart = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetDispatchFetchEventEnd(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mDispatchFetchEventEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetDispatchFetchEventEnd(TimeStamp aTimeStamp) {
  mDispatchFetchEventEnd = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetHandleFetchEventStart(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mHandleFetchEventStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetHandleFetchEventStart(TimeStamp aTimeStamp) {
  mHandleFetchEventStart = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetHandleFetchEventEnd(TimeStamp* _retval) {
  MOZ_ASSERT(_retval);
  *_retval = mHandleFetchEventEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetHandleFetchEventEnd(TimeStamp aTimeStamp) {
  mHandleFetchEventEnd = aTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetDomainLookupStart(TimeStamp* _retval) {
  *_retval = mTransactionTimings.domainLookupStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetDomainLookupEnd(TimeStamp* _retval) {
  *_retval = mTransactionTimings.domainLookupEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetConnectStart(TimeStamp* _retval) {
  *_retval = mTransactionTimings.connectStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTcpConnectEnd(TimeStamp* _retval) {
  *_retval = mTransactionTimings.tcpConnectEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetSecureConnectionStart(TimeStamp* _retval) {
  *_retval = mTransactionTimings.secureConnectionStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetConnectEnd(TimeStamp* _retval) {
  *_retval = mTransactionTimings.connectEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestStart(TimeStamp* _retval) {
  *_retval = mTransactionTimings.requestStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStart(TimeStamp* _retval) {
  *_retval = mTransactionTimings.responseStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseEnd(TimeStamp* _retval) {
  *_retval = mTransactionTimings.responseEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCacheReadStart(TimeStamp* _retval) {
  *_retval = mCacheReadStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCacheReadEnd(TimeStamp* _retval) {
  *_retval = mCacheReadEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetInitiatorType(nsAString& aInitiatorType) {
  aInitiatorType = mInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetInitiatorType(const nsAString& aInitiatorType) {
  mInitiatorType = aInitiatorType;
  return NS_OK;
}

#define IMPL_TIMING_ATTR(name)                                           \
  NS_IMETHODIMP                                                          \
  HttpBaseChannel::Get##name##Time(PRTime* _retval) {                    \
    TimeStamp stamp;                                                     \
    Get##name(&stamp);                                                   \
    if (stamp.IsNull()) {                                                \
      *_retval = 0;                                                      \
      return NS_OK;                                                      \
    }                                                                    \
    *_retval =                                                           \
        mChannelCreationTime +                                           \
        (PRTime)((stamp - mChannelCreationTimestamp).ToSeconds() * 1e6); \
    return NS_OK;                                                        \
  }

IMPL_TIMING_ATTR(ChannelCreation)
IMPL_TIMING_ATTR(AsyncOpen)
IMPL_TIMING_ATTR(LaunchServiceWorkerStart)
IMPL_TIMING_ATTR(LaunchServiceWorkerEnd)
IMPL_TIMING_ATTR(DispatchFetchEventStart)
IMPL_TIMING_ATTR(DispatchFetchEventEnd)
IMPL_TIMING_ATTR(HandleFetchEventStart)
IMPL_TIMING_ATTR(HandleFetchEventEnd)
IMPL_TIMING_ATTR(DomainLookupStart)
IMPL_TIMING_ATTR(DomainLookupEnd)
IMPL_TIMING_ATTR(ConnectStart)
IMPL_TIMING_ATTR(TcpConnectEnd)
IMPL_TIMING_ATTR(SecureConnectionStart)
IMPL_TIMING_ATTR(ConnectEnd)
IMPL_TIMING_ATTR(RequestStart)
IMPL_TIMING_ATTR(ResponseStart)
IMPL_TIMING_ATTR(ResponseEnd)
IMPL_TIMING_ATTR(CacheReadStart)
IMPL_TIMING_ATTR(CacheReadEnd)
IMPL_TIMING_ATTR(RedirectStart)
IMPL_TIMING_ATTR(RedirectEnd)

#undef IMPL_TIMING_ATTR

mozilla::dom::PerformanceStorage* HttpBaseChannel::GetPerformanceStorage() {
  // If performance timing is disabled, there is no need for the Performance
  // object anymore.
  if (!LoadTimingEnabled()) {
    return nullptr;
  }

  // There is no point in continuing, since the performance object in the parent
  // isn't the same as the one in the child which will be reporting resource
  // performance.
  if (XRE_IsE10sParentProcess()) {
    return nullptr;
  }
  return mLoadInfo->GetPerformanceStorage();
}

void HttpBaseChannel::MaybeReportTimingData() {
  if (XRE_IsE10sParentProcess()) {
    return;
  }

  mozilla::dom::PerformanceStorage* documentPerformance =
      GetPerformanceStorage();
  if (documentPerformance) {
    documentPerformance->AddEntry(this, this);
    return;
  }

  if (!nsGlobalWindowInner::GetInnerWindowWithId(
          mLoadInfo->GetInnerWindowID())) {
    // The inner window is in a different process.
    dom::ContentChild* child = dom::ContentChild::GetSingleton();

    if (!child) {
      return;
    }
    nsAutoString initiatorType;
    nsAutoString entryName;

    UniquePtr<dom::PerformanceTimingData> performanceTimingData(
        dom::PerformanceTimingData::Create(this, this, 0, initiatorType,
                                           entryName));
    if (!performanceTimingData) {
      return;
    }
    child->SendReportFrameTimingData(mLoadInfo->GetInnerWindowID(), entryName,
                                     initiatorType,
                                     std::move(performanceTimingData));
  }
}

NS_IMETHODIMP
HttpBaseChannel::SetReportResourceTiming(bool enabled) {
  StoreReportTiming(enabled);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetReportResourceTiming(bool* _retval) {
  *_retval = LoadReportTiming();
  return NS_OK;
}

nsIURI* HttpBaseChannel::GetReferringPage() {
  nsCOMPtr<nsPIDOMWindowInner> pDomWindow = GetInnerDOMWindow();
  if (!pDomWindow) {
    return nullptr;
  }
  return pDomWindow->GetDocumentURI();
}

nsPIDOMWindowInner* HttpBaseChannel::GetInnerDOMWindow() {
  nsCOMPtr<nsILoadContext> loadContext;
  NS_QueryNotificationCallbacks(this, loadContext);
  if (!loadContext) {
    return nullptr;
  }
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  loadContext->GetAssociatedWindow(getter_AddRefs(domWindow));
  if (!domWindow) {
    return nullptr;
  }
  auto* pDomWindow = nsPIDOMWindowOuter::From(domWindow);
  if (!pDomWindow) {
    return nullptr;
  }
  nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      pDomWindow->GetCurrentInnerWindow();
  if (!innerWindow) {
    return nullptr;
  }

  return innerWindow;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIThrottledInputChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetThrottleQueue(nsIInputChannelThrottleQueue* aQueue) {
  if (!XRE_IsParentProcess()) {
    return NS_ERROR_FAILURE;
  }

  mThrottleQueue = aQueue;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetThrottleQueue(nsIInputChannelThrottleQueue** aQueue) {
  NS_ENSURE_ARG_POINTER(aQueue);
  nsCOMPtr<nsIInputChannelThrottleQueue> queue = mThrottleQueue;
  queue.forget(aQueue);
  return NS_OK;
}

//------------------------------------------------------------------------------

bool HttpBaseChannel::EnsureRequestContextID() {
  if (mRequestContextID) {
    // Already have a request context ID, no need to do the rest of this work
    LOG(("HttpBaseChannel::EnsureRequestContextID this=%p id=%" PRIx64, this,
         mRequestContextID));
    return true;
  }

  // Find the loadgroup at the end of the chain in order
  // to make sure all channels derived from the load group
  // use the same connection scope.
  nsCOMPtr<nsILoadGroupChild> childLoadGroup = do_QueryInterface(mLoadGroup);
  if (!childLoadGroup) {
    return false;
  }

  nsCOMPtr<nsILoadGroup> rootLoadGroup;
  childLoadGroup->GetRootLoadGroup(getter_AddRefs(rootLoadGroup));
  if (!rootLoadGroup) {
    return false;
  }

  // Set the load group connection scope on this channel and its transaction
  rootLoadGroup->GetRequestContextID(&mRequestContextID);

  LOG(("HttpBaseChannel::EnsureRequestContextID this=%p id=%" PRIx64, this,
       mRequestContextID));

  return true;
}

bool HttpBaseChannel::EnsureRequestContext() {
  if (mRequestContext) {
    // Already have a request context, no need to do the rest of this work
    return true;
  }

  if (!EnsureRequestContextID()) {
    return false;
  }

  nsIRequestContextService* rcsvc = gHttpHandler->GetRequestContextService();
  if (!rcsvc) {
    return false;
  }

  rcsvc->GetRequestContext(mRequestContextID, getter_AddRefs(mRequestContext));
  return static_cast<bool>(mRequestContext);
}

void HttpBaseChannel::EnsureTopBrowsingContextId() {
  if (mTopBrowsingContextId) {
    return;
  }

  RefPtr<dom::BrowsingContext> bc;
  MOZ_ALWAYS_SUCCEEDS(mLoadInfo->GetBrowsingContext(getter_AddRefs(bc)));

  if (bc && bc->Top()) {
    mTopBrowsingContextId = bc->Top()->Id();
  }
}

void HttpBaseChannel::InitiateORBTelemetry() {
  MOZ_ASSERT(!mOpaqueResponseBlockingInfo);
  MOZ_RELEASE_ASSERT(mLoadInfo);

  mOpaqueResponseBlockingInfo = MakeRefPtr<OpaqueResponseBlockingInfo>(
      mLoadInfo->GetExternalContentPolicyType());
}

void HttpBaseChannel::ReportORBTelemetry(const nsCString& aKey) {
  MOZ_ASSERT(mOpaqueResponseBlockingInfo);

  mOpaqueResponseBlockingInfo->Report(aKey);
  mOpaqueResponseBlockingInfo = nullptr;
}

void HttpBaseChannel::ReportORBTelemetry(int64_t aContentLength) {
  MOZ_ASSERT(mOpaqueResponseBlockingInfo);

  mOpaqueResponseBlockingInfo->ReportContentLength(aContentLength);
}

void HttpBaseChannel::SetCorsPreflightParameters(
    const nsTArray<nsCString>& aUnsafeHeaders,
    bool aShouldStripRequestBodyHeader) {
  MOZ_RELEASE_ASSERT(!LoadRequestObserversCalled());

  StoreRequireCORSPreflight(true);
  mUnsafeHeaders = aUnsafeHeaders.Clone();
  if (aShouldStripRequestBodyHeader) {
    mUnsafeHeaders.RemoveElementsBy([&](const nsCString& aHeader) {
      return aHeader.LowerCaseEqualsASCII("content-type") ||
             aHeader.LowerCaseEqualsASCII("content-encoding") ||
             aHeader.LowerCaseEqualsASCII("content-language") ||
             aHeader.LowerCaseEqualsASCII("content-location");
    });
  }
}

void HttpBaseChannel::SetAltDataForChild(bool aIsForChild) {
  StoreAltDataForChild(aIsForChild);
}

NS_IMETHODIMP
HttpBaseChannel::GetBlockAuthPrompt(bool* aValue) {
  if (!aValue) {
    return NS_ERROR_FAILURE;
  }

  *aValue = LoadBlockAuthPrompt();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetBlockAuthPrompt(bool aValue) {
  ENSURE_CALLED_BEFORE_CONNECT();

  StoreBlockAuthPrompt(aValue);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetConnectionInfoHashKey(nsACString& aConnectionInfoHashKey) {
  if (!mConnectionInfo) {
    return NS_ERROR_FAILURE;
  }
  aConnectionInfoHashKey.Assign(mConnectionInfo->HashKey());
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLastRedirectFlags(uint32_t* aValue) {
  NS_ENSURE_ARG(aValue);
  *aValue = mLastRedirectFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLastRedirectFlags(uint32_t aValue) {
  mLastRedirectFlags = aValue;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetNavigationStartTimeStamp(TimeStamp* aTimeStamp) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpBaseChannel::SetNavigationStartTimeStamp(TimeStamp aTimeStamp) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult HttpBaseChannel::CheckRedirectLimit(uint32_t aRedirectFlags) const {
  if (aRedirectFlags & nsIChannelEventSink::REDIRECT_INTERNAL) {
    // Some platform features, like Service Workers, depend on internal
    // redirects.  We should allow some number of internal redirects above
    // and beyond the normal redirect limit so these features continue
    // to work.
    static const int8_t kMinInternalRedirects = 5;

    if (mInternalRedirectCount >= (mRedirectionLimit + kMinInternalRedirects)) {
      LOG(("internal redirection limit reached!\n"));
      return NS_ERROR_REDIRECT_LOOP;
    }
    return NS_OK;
  }

  MOZ_ASSERT(aRedirectFlags & (nsIChannelEventSink::REDIRECT_TEMPORARY |
                               nsIChannelEventSink::REDIRECT_PERMANENT |
                               nsIChannelEventSink::REDIRECT_STS_UPGRADE));

  if (mRedirectCount >= mRedirectionLimit) {
    LOG(("redirection limit reached!\n"));
    return NS_ERROR_REDIRECT_LOOP;
  }

  // in case https-only mode is enabled which upgrades top-level requests to
  // https and the page answers with a redirect (meta, 302, win.location, ...)
  // then this method can break the cycle which causes the https-only exception
  // page to appear. Note that https-first mode breaks upgrade downgrade endless
  // loops within ShouldUpgradeHTTPSFirstRequest because https-first does not
  // display an exception page but needs a soft fallback/downgrade.
  if (nsHTTPSOnlyUtils::IsUpgradeDowngradeEndlessLoop(
          mURI, mLoadInfo,
          {nsHTTPSOnlyUtils::UpgradeDowngradeEndlessLoopOptions::
               EnforceForHTTPSOnlyMode})) {
    LOG(("upgrade downgrade redirect loop!\n"));
    return NS_ERROR_REDIRECT_LOOP;
  }

  return NS_OK;
}

// NOTE: This function duplicates code from nsBaseChannel. This will go away
// once HTTP uses nsBaseChannel (part of bug 312760)
/* static */
void HttpBaseChannel::CallTypeSniffers(void* aClosure, const uint8_t* aData,
                                       uint32_t aCount) {
  nsIChannel* chan = static_cast<nsIChannel*>(aClosure);
  const char* snifferType = [chan]() {
    if (RefPtr<nsHttpChannel> httpChannel = do_QueryObject(chan)) {
      switch (httpChannel->GetSnifferCategoryType()) {
        case SnifferCategoryType::NetContent:
          return NS_CONTENT_SNIFFER_CATEGORY;
        case SnifferCategoryType::OpaqueResponseBlocking:
          return NS_ORB_SNIFFER_CATEGORY;
        case SnifferCategoryType::All:
          return NS_CONTENT_AND_ORB_SNIFFER_CATEGORY;
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected SnifferCategoryType!");
      }
    }

    return NS_CONTENT_SNIFFER_CATEGORY;
  }();

  nsAutoCString newType;
  NS_SniffContent(snifferType, chan, aData, aCount, newType);
  if (!newType.IsEmpty()) {
    chan->SetContentType(newType);
  }
}

template <class T>
static void ParseServerTimingHeader(
    const UniquePtr<T>& aHeader, nsTArray<nsCOMPtr<nsIServerTiming>>& aOutput) {
  if (!aHeader) {
    return;
  }

  nsAutoCString serverTimingHeader;
  Unused << aHeader->GetHeader(nsHttp::Server_Timing, serverTimingHeader);
  if (serverTimingHeader.IsEmpty()) {
    return;
  }

  ServerTimingParser parser(serverTimingHeader);
  parser.Parse();

  nsTArray<nsCOMPtr<nsIServerTiming>> array = parser.TakeServerTimingHeaders();
  aOutput.AppendElements(array);
}

NS_IMETHODIMP
HttpBaseChannel::GetServerTiming(nsIArray** aServerTiming) {
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aServerTiming);

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsCOMPtr<nsIServerTiming>> data;
  rv = GetNativeServerTiming(data);
  NS_ENSURE_SUCCESS(rv, rv);

  for (const auto& entry : data) {
    array->AppendElement(entry);
  }

  array.forget(aServerTiming);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetNativeServerTiming(
    nsTArray<nsCOMPtr<nsIServerTiming>>& aServerTiming) {
  aServerTiming.Clear();

  if (nsContentUtils::ComputeIsSecureContext(this)) {
    ParseServerTimingHeader(mResponseHead, aServerTiming);
    ParseServerTimingHeader(mResponseTrailers, aServerTiming);
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::CancelByURLClassifier(nsresult aErrorCode) {
  MOZ_ASSERT(
      UrlClassifierFeatureFactory::IsClassifierBlockingErrorCode(aErrorCode));
  return Cancel(aErrorCode);
}

void HttpBaseChannel::SetIPv4Disabled() { mCaps |= NS_HTTP_DISABLE_IPV4; }

void HttpBaseChannel::SetIPv6Disabled() { mCaps |= NS_HTTP_DISABLE_IPV6; }

NS_IMETHODIMP HttpBaseChannel::GetResponseEmbedderPolicy(
    nsILoadInfo::CrossOriginEmbedderPolicy* aOutPolicy) {
  *aOutPolicy = nsILoadInfo::EMBEDDER_POLICY_NULL;
  if (!mResponseHead) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!nsContentUtils::ComputeIsSecureContext(this)) {
    // Feature is only available for secure contexts.
    return NS_OK;
  }

  nsAutoCString content;
  Unused << mResponseHead->GetHeader(nsHttp::Cross_Origin_Embedder_Policy,
                                     content);

  *aOutPolicy = NS_GetCrossOriginEmbedderPolicyFromHeader(content);
  return NS_OK;
}

// Obtain a cross-origin opener-policy from a response response and a
// cross-origin opener policy initiator.
// https://gist.github.com/annevk/6f2dd8c79c77123f39797f6bdac43f3e
NS_IMETHODIMP HttpBaseChannel::ComputeCrossOriginOpenerPolicy(
    nsILoadInfo::CrossOriginOpenerPolicy aInitiatorPolicy,
    nsILoadInfo::CrossOriginOpenerPolicy* aOutPolicy) {
  MOZ_ASSERT(aOutPolicy);
  *aOutPolicy = nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;

  if (!mResponseHead) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // COOP headers are ignored for insecure-context loads.
  if (!nsContentUtils::ComputeIsSecureContext(this)) {
    return NS_OK;
  }

  nsAutoCString openerPolicy;
  Unused << mResponseHead->GetHeader(nsHttp::Cross_Origin_Opener_Policy,
                                     openerPolicy);

  // Cross-Origin-Opener-Policy = %s"same-origin" /
  //                              %s"same-origin-allow-popups" /
  //                              %s"unsafe-none"; case-sensitive

  nsCOMPtr<nsISFVService> sfv = GetSFVService();

  nsCOMPtr<nsISFVItem> item;
  nsresult rv = sfv->ParseItem(openerPolicy, getter_AddRefs(item));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISFVBareItem> value;
  rv = item->GetValue(getter_AddRefs(value));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsISFVToken> token = do_QueryInterface(value);
  if (!token) {
    return NS_ERROR_UNEXPECTED;
  }

  rv = token->GetValue(openerPolicy);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsILoadInfo::CrossOriginOpenerPolicy policy =
      nsILoadInfo::OPENER_POLICY_UNSAFE_NONE;

  if (openerPolicy.EqualsLiteral("same-origin")) {
    policy = nsILoadInfo::OPENER_POLICY_SAME_ORIGIN;
  } else if (openerPolicy.EqualsLiteral("same-origin-allow-popups")) {
    policy = nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_ALLOW_POPUPS;
  }

  if (policy == nsILoadInfo::OPENER_POLICY_SAME_ORIGIN) {
    nsILoadInfo::CrossOriginEmbedderPolicy coep =
        nsILoadInfo::EMBEDDER_POLICY_NULL;
    if (NS_SUCCEEDED(GetResponseEmbedderPolicy(&coep)) &&
        coep == nsILoadInfo::EMBEDDER_POLICY_REQUIRE_CORP) {
      policy =
          nsILoadInfo::OPENER_POLICY_SAME_ORIGIN_EMBEDDER_POLICY_REQUIRE_CORP;
    }
  }

  *aOutPolicy = policy;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCrossOriginOpenerPolicy(
    nsILoadInfo::CrossOriginOpenerPolicy* aPolicy) {
  MOZ_ASSERT(aPolicy);
  if (!aPolicy) {
    return NS_ERROR_INVALID_ARG;
  }
  // If this method is called before OnStartRequest (ie. before we call
  // ComputeCrossOriginOpenerPolicy) or if we were unable to compute the
  // policy we'll throw an error.
  if (!LoadOnStartRequestCalled()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aPolicy = mComputedCrossOriginOpenerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::HasCrossOriginOpenerPolicyMismatch(bool* aIsMismatch) {
  // This should only be called in parent process.
  MOZ_ASSERT(XRE_IsParentProcess());
  *aIsMismatch = LoadHasCrossOriginOpenerPolicyMismatch();
  return NS_OK;
}

void HttpBaseChannel::MaybeFlushConsoleReports() {
  // If this channel is part of a loadGroup, we can flush the console reports
  // immediately.
  nsCOMPtr<nsILoadGroup> loadGroup;
  nsresult rv = GetLoadGroup(getter_AddRefs(loadGroup));
  if (NS_SUCCEEDED(rv) && loadGroup) {
    FlushConsoleReports(loadGroup);
  }
}

void HttpBaseChannel::DoDiagnosticAssertWhenOnStopNotCalledOnDestroy() {}

NS_IMETHODIMP HttpBaseChannel::SetWaitForHTTPSSVCRecord() {
  mCaps |= NS_HTTP_FORCE_WAIT_HTTP_RR;
  return NS_OK;
}

bool HttpBaseChannel::Http3Allowed() const {
  bool isDirectOrNoProxy =
      mProxyInfo ? static_cast<nsProxyInfo*>(mProxyInfo.get())->IsDirect()
                 : true;
  return !mUpgradeProtocolCallback && isDirectOrNoProxy &&
         !(mCaps & NS_HTTP_BE_CONSERVATIVE) && !LoadBeConservative() &&
         LoadAllowHttp3();
}

void HttpBaseChannel::SetDummyChannelForImageCache() {
  mDummyChannelForImageCache = true;
  MOZ_ASSERT(!mResponseHead,
             "SetDummyChannelForImageCache should only be called once");
  mResponseHead = MakeUnique<nsHttpResponseHead>();
}

void HttpBaseChannel::SetConnectionInfo(nsHttpConnectionInfo* aCI) {
  mConnectionInfo = aCI ? aCI->Clone() : nullptr;
}

}  // namespace net
}  // namespace mozilla
