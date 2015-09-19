/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "mozilla/net/HttpBaseChannel.h"

#include "nsHttpHandler.h"
#include "nsMimeTypes.h"
#include "nsNetCID.h"
#include "nsNetUtil.h"

#include "nsICachingChannel.h"
#include "nsIPrincipal.h"
#include "nsIScriptError.h"
#include "nsISeekableStream.h"
#include "nsIStorageStream.h"
#include "nsITimedChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIApplicationCacheChannel.h"
#include "nsIMutableArray.h"
#include "nsEscape.h"
#include "nsStreamListenerWrapper.h"
#include "nsISecurityConsoleMessage.h"
#include "nsURLHelper.h"
#include "nsICookieService.h"
#include "nsIStreamConverterService.h"
#include "nsCRT.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"
#include "nsIObserverService.h"
#include "nsProxyRelease.h"
#include "nsPIDOMWindow.h"
#include "nsPerformance.h"
#include "nsINetworkInterceptController.h"
#include "mozIThirdPartyUtil.h"
#include "nsStreamUtils.h"
#include "nsContentSecurityManager.h"
#include "nsILoadGroupChild.h"

#include <algorithm>

namespace mozilla {
namespace net {

HttpBaseChannel::HttpBaseChannel()
  : mStartPos(UINT64_MAX)
  , mStatus(NS_OK)
  , mLoadFlags(LOAD_NORMAL)
  , mCaps(0)
  , mClassOfService(0)
  , mPriority(PRIORITY_NORMAL)
  , mRedirectionLimit(gHttpHandler->RedirectionLimit())
  , mApplyConversion(true)
  , mCanceled(false)
  , mIsPending(false)
  , mWasOpened(false)
  , mRequestObserversCalled(false)
  , mResponseHeadersModified(false)
  , mAllowPipelining(true)
  , mAllowSTS(true)
  , mThirdPartyFlags(0)
  , mUploadStreamHasHeaders(false)
  , mInheritApplicationCache(true)
  , mChooseApplicationCache(false)
  , mLoadedFromApplicationCache(false)
  , mChannelIsForDownload(false)
  , mTracingEnabled(true)
  , mTimingEnabled(false)
  , mAllowSpdy(true)
  , mAllowAltSvc(true)
  , mResponseTimeoutEnabled(true)
  , mAllRedirectsSameOrigin(true)
  , mAllRedirectsPassTimingAllowCheck(true)
  , mForceNoIntercept(false)
  , mResponseCouldBeSynthesized(false)
  , mSuspendCount(0)
  , mInitialRwin(0)
  , mProxyResolveFlags(0)
  , mProxyURI(nullptr)
  , mContentDispositionHint(UINT32_MAX)
  , mHttpHandler(gHttpHandler)
  , mReferrerPolicy(REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE)
  , mRedirectCount(0)
  , mForcePending(false)
  , mCorsIncludeCredentials(false)
  , mCorsMode(nsIHttpChannelInternal::CORS_MODE_NO_CORS)
  , mRedirectMode(nsIHttpChannelInternal::REDIRECT_MODE_FOLLOW)
  , mOnStartRequestCalled(false)
  , mRequireCORSPreflight(false)
  , mWithCredentials(false)
{
  LOG(("Creating HttpBaseChannel @%x\n", this));

  // Subfields of unions cannot be targeted in an initializer list.
#ifdef MOZ_VALGRIND
  // Zero the entire unions so that Valgrind doesn't complain when we send them
  // to another process.
  memset(&mSelfAddr, 0, sizeof(NetAddr));
  memset(&mPeerAddr, 0, sizeof(NetAddr));
#endif
  mSelfAddr.raw.family = PR_AF_UNSPEC;
  mPeerAddr.raw.family = PR_AF_UNSPEC;
  mSchedulingContextID.Clear();
}

HttpBaseChannel::~HttpBaseChannel()
{
  LOG(("Destroying HttpBaseChannel @%x\n", this));

  NS_ReleaseOnMainThread(mLoadInfo);

  // Make sure we don't leak
  CleanRedirectCacheChainIfNecessary();
}

nsresult
HttpBaseChannel::Init(nsIURI *aURI,
                      uint32_t aCaps,
                      nsProxyInfo *aProxyInfo,
                      uint32_t aProxyResolveFlags,
                      nsIURI *aProxyURI)
{
  LOG(("HttpBaseChannel::Init [this=%p]\n", this));

  NS_PRECONDITION(aURI, "null uri");

  mURI = aURI;
  mOriginalURI = aURI;
  mDocumentURI = nullptr;
  mCaps = aCaps;
  mProxyResolveFlags = aProxyResolveFlags;
  mProxyURI = aProxyURI;

  // Construct connection info object
  nsAutoCString host;
  int32_t port = -1;
  bool isHTTPS = false;

  nsresult rv = mURI->SchemeIs("https", &isHTTPS);
  if (NS_FAILED(rv)) return rv;

  rv = mURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  // Reject the URL if it doesn't specify a host
  if (host.IsEmpty())
    return NS_ERROR_MALFORMED_URI;

  rv = mURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  LOG(("host=%s port=%d\n", host.get(), port));

  rv = mURI->GetAsciiSpec(mSpec);
  if (NS_FAILED(rv)) return rv;
  LOG(("uri=%s\n", mSpec.get()));

  // Assert default request method
  MOZ_ASSERT(mRequestHead.EqualsMethod(nsHttpRequestHead::kMethod_Get));

  // Set request headers
  nsAutoCString hostLine;
  rv = nsHttpHandler::GenerateHostPort(host, port, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = mRequestHead.SetHeader(nsHttp::Host, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = gHttpHandler->AddStandardRequestHeaders(&mRequestHead.Headers(), isHTTPS);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString type;
  if (aProxyInfo && NS_SUCCEEDED(aProxyInfo->GetType(type)) &&
      !type.EqualsLiteral("unknown"))
    mProxyInfo = aProxyInfo;

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
  NS_INTERFACE_MAP_ENTRY(nsIEncodedChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannel)
  NS_INTERFACE_MAP_ENTRY(nsIHttpChannelInternal)
  NS_INTERFACE_MAP_ENTRY(nsIForcePendingChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel)
  NS_INTERFACE_MAP_ENTRY(nsIUploadChannel2)
  NS_INTERFACE_MAP_ENTRY(nsISupportsPriority)
  NS_INTERFACE_MAP_ENTRY(nsITraceableChannel)
  NS_INTERFACE_MAP_ENTRY(nsIPrivateBrowsingChannel)
  NS_INTERFACE_MAP_ENTRY(nsITimedChannel)
NS_INTERFACE_MAP_END_INHERITING(nsHashPropertyBag)

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIRequest
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetName(nsACString& aName)
{
  aName = mSpec;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsPending(bool *aIsPending)
{
  NS_ENSURE_ARG_POINTER(aIsPending);
  *aIsPending = mIsPending || mForcePending;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetStatus(nsresult *aStatus)
{
  NS_ENSURE_ARG_POINTER(aStatus);
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  NS_ENSURE_ARG_POINTER(aLoadGroup);
  *aLoadGroup = mLoadGroup;
  NS_IF_ADDREF(*aLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  
  if (!CanSetLoadGroup(aLoadGroup)) {
    return NS_ERROR_FAILURE;
  }

  mLoadGroup = aLoadGroup;
  mProgressSink = nullptr;
  mPrivateBrowsing = NS_UsePrivateBrowsing(this);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  NS_ENSURE_ARG_POINTER(aLoadFlags);
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetOriginalURI(nsIURI **aOriginalURI)
{
  NS_ENSURE_ARG_POINTER(aOriginalURI);
  *aOriginalURI = mOriginalURI;
  NS_ADDREF(*aOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetOriginalURI(nsIURI *aOriginalURI)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  NS_ENSURE_ARG_POINTER(aOriginalURI);
  mOriginalURI = aOriginalURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetURI(nsIURI **aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  *aURI = mURI;
  NS_ADDREF(*aURI);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetOwner(nsISupports **aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(*aOwner);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetOwner(nsISupports *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadInfo(nsILoadInfo *aLoadInfo)
{
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadInfo(nsILoadInfo **aLoadInfo)
{
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetNotificationCallbacks(nsIInterfaceRequestor **aCallbacks)
{
  *aCallbacks = mCallbacks;
  NS_IF_ADDREF(*aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetNotificationCallbacks(nsIInterfaceRequestor *aCallbacks)
{
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  
  if (!CanSetCallbacks(aCallbacks)) {
    return NS_ERROR_FAILURE;
  }

  mCallbacks = aCallbacks;
  mProgressSink = nullptr;

  mPrivateBrowsing = NS_UsePrivateBrowsing(this);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentType(nsACString& aContentType)
{
  if (!mResponseHead) {
    aContentType.Truncate();
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (!mResponseHead->ContentType().IsEmpty()) {
    aContentType = mResponseHead->ContentType();
    return NS_OK;
  }

  aContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentType(const nsACString& aContentType)
{
  if (mListener || mWasOpened) {
    if (!mResponseHead)
      return NS_ERROR_NOT_AVAILABLE;

    nsAutoCString contentTypeBuf, charsetBuf;
    bool hadCharset;
    net_ParseContentType(aContentType, contentTypeBuf, charsetBuf, &hadCharset);

    mResponseHead->SetContentType(contentTypeBuf);

    // take care not to stomp on an existing charset
    if (hadCharset)
      mResponseHead->SetContentCharset(charsetBuf);

  } else {
    // We are being given a content-type hint.
    bool dummy;
    net_ParseContentType(aContentType, mContentTypeHint, mContentCharsetHint,
                         &dummy);
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentCharset(nsACString& aContentCharset)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  aContentCharset = mResponseHead->ContentCharset();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentCharset(const nsACString& aContentCharset)
{
  if (mListener) {
    if (!mResponseHead)
      return NS_ERROR_NOT_AVAILABLE;

    mResponseHead->SetContentCharset(aContentCharset);
  } else {
    // Charset hint
    mContentCharsetHint = aContentCharset;
  }
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDisposition(uint32_t *aContentDisposition)
{
  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_FAILED(rv)) {
    if (mContentDispositionHint == UINT32_MAX)
      return rv;

    *aContentDisposition = mContentDispositionHint;
    return NS_OK;
  }

  *aContentDisposition = NS_GetContentDispositionFromHeader(header, this);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentDisposition(uint32_t aContentDisposition)
{
  mContentDispositionHint = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDispositionFilename(nsAString& aContentDispositionFilename)
{
  aContentDispositionFilename.Truncate();
  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_FAILED(rv)) {
    if (!mContentDispositionFilename)
      return rv;

    aContentDispositionFilename = *mContentDispositionFilename;
    return NS_OK;
  }

  return NS_GetFilenameFromDisposition(aContentDispositionFilename,
                                       header, mURI);
}

NS_IMETHODIMP
HttpBaseChannel::SetContentDispositionFilename(const nsAString& aContentDispositionFilename)
{
  mContentDispositionFilename = new nsString(aContentDispositionFilename);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDispositionHeader(nsACString& aContentDispositionHeader)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  nsresult rv = mResponseHead->GetHeader(nsHttp::Content_Disposition,
                                         aContentDispositionHeader);
  if (NS_FAILED(rv) || aContentDispositionHeader.IsEmpty())
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentLength(int64_t *aContentLength)
{
  NS_ENSURE_ARG_POINTER(aContentLength);

  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  *aContentLength = mResponseHead->ContentLength();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentLength(int64_t value)
{
  NS_NOTYETIMPLEMENTED("HttpBaseChannel::SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
HttpBaseChannel::Open(nsIInputStream **aResult)
{
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_IN_PROGRESS);
  return NS_ImplementChannelOpen(this, aResult);
}

NS_IMETHODIMP
HttpBaseChannel::Open2(nsIInputStream** aStream)
{
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv = nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);
  return Open(aStream);
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIUploadChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetUploadStream(nsIInputStream **stream)
{
  NS_ENSURE_ARG_POINTER(stream);
  *stream = mUploadStream;
  NS_IF_ADDREF(*stream);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetUploadStream(nsIInputStream *stream,
                               const nsACString &contentType,
                               int64_t contentLength)
{
  // NOTE: for backwards compatibility and for compatibility with old style
  // plugins, |stream| may include headers, specifically Content-Type and
  // Content-Length headers.  in this case, |contentType| and |contentLength|
  // would be unspecified.  this is traditionally the case of a POST request,
  // and so we select POST as the request method if contentType and
  // contentLength are unspecified.

  if (stream) {
    nsAutoCString method;
    bool hasHeaders;

    if (contentType.IsEmpty()) {
      method = NS_LITERAL_CSTRING("POST");
      hasHeaders = true;
    } else {
      method = NS_LITERAL_CSTRING("PUT");
      hasHeaders = false;
    }
    return ExplicitSetUploadStream(stream, contentType, contentLength,
                                   method, hasHeaders);
  }

  // if stream is null, ExplicitSetUploadStream returns error.
  // So we need special case for GET method.
  mUploadStreamHasHeaders = false;
  mRequestHead.SetMethod(NS_LITERAL_CSTRING("GET")); // revert to GET request
  mUploadStream = stream;
  return NS_OK;
}

namespace {

void
CopyComplete(void* aClosure, nsresult aStatus) {
  // Called on the STS thread by NS_AsyncCopy
  auto channel = static_cast<HttpBaseChannel*>(aClosure);
  nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableMethodWithArg<nsresult>(
    channel, &HttpBaseChannel::EnsureUploadStreamIsCloneableComplete, aStatus);
  NS_DispatchToMainThread(runnable.forget());
}

} // anonymous namespace

NS_IMETHODIMP
HttpBaseChannel::EnsureUploadStreamIsCloneable(nsIRunnable* aCallback)
{
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  NS_ENSURE_ARG_POINTER(aCallback);

  // We could in theory allow multiple callers to use this method,
  // but the complexity does not seem worth it yet.  Just fail if
  // this is called more than once simultaneously.
  NS_ENSURE_FALSE(mUploadCloneableCallback, NS_ERROR_UNEXPECTED);

  // If the CloneUploadStream() will succeed, then synchronously invoke
  // the callback to indicate we're already cloneable.
  if (!mUploadStream || NS_InputStreamIsCloneable(mUploadStream)) {
    aCallback->Run();
    return NS_OK;
  }

  nsCOMPtr<nsIStorageStream> storageStream;
  nsresult rv = NS_NewStorageStream(4096, UINT32_MAX,
                                    getter_AddRefs(storageStream));
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
    rv = NS_NewBufferedInputStream(getter_AddRefs(source), mUploadStream, 4096);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIEventTarget> target =
    do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);

  mUploadCloneableCallback = aCallback;

  rv = NS_AsyncCopy(source, sink, target, NS_ASYNCCOPY_VIA_READSEGMENTS,
                    4096, // copy segment size
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

void
HttpBaseChannel::EnsureUploadStreamIsCloneableComplete(nsresult aStatus)
{
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
HttpBaseChannel::CloneUploadStream(nsIInputStream** aClonedStream)
{
  NS_ENSURE_ARG_POINTER(aClonedStream);
  *aClonedStream = nullptr;

  if (!mUploadStream) {
    return NS_OK;
  }

  nsCOMPtr<nsIInputStream> clonedStream;
  nsresult rv = NS_CloneInputStream(mUploadStream, getter_AddRefs(clonedStream));
  NS_ENSURE_SUCCESS(rv, rv);

  clonedStream.forget(aClonedStream);

  return NS_OK;
}


//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIUploadChannel2
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::ExplicitSetUploadStream(nsIInputStream *aStream,
                                       const nsACString &aContentType,
                                       int64_t aContentLength,
                                       const nsACString &aMethod,
                                       bool aStreamHasHeaders)
{
  // Ensure stream is set and method is valid
  NS_ENSURE_TRUE(aStream, NS_ERROR_FAILURE);

  if (aContentLength < 0 && !aStreamHasHeaders) {
    nsresult rv = aStream->Available(reinterpret_cast<uint64_t*>(&aContentLength));
    if (NS_FAILED(rv) || aContentLength < 0) {
      NS_ERROR("unable to determine content length");
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = SetRequestMethod(aMethod);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aStreamHasHeaders) {
    // SetRequestHeader propagates headers to chrome if HttpChannelChild
    nsAutoCString contentLengthStr;
    contentLengthStr.AppendInt(aContentLength);
    SetRequestHeader(NS_LITERAL_CSTRING("Content-Length"), contentLengthStr,
                     false);
    SetRequestHeader(NS_LITERAL_CSTRING("Content-Type"), aContentType,
                     false);
  }

  mUploadStreamHasHeaders = aStreamHasHeaders;
  mUploadStream = aStream;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetUploadStreamHasHeaders(bool *hasHeaders)
{
  NS_ENSURE_ARG(hasHeaders);

  *hasHeaders = mUploadStreamHasHeaders;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIEncodedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetApplyConversion(bool *value)
{
  *value = mApplyConversion;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetApplyConversion(bool value)
{
  LOG(("HttpBaseChannel::SetApplyConversion [this=%p value=%d]\n", this, value));
  mApplyConversion = value;
  return NS_OK;
}

nsresult
HttpBaseChannel::DoApplyContentConversions(nsIStreamListener* aNextListener,
                                           nsIStreamListener** aNewNextListener)
{
  return DoApplyContentConversions(aNextListener,
                                   aNewNextListener,
                                   mListenerContext);
}

NS_IMETHODIMP
HttpBaseChannel::DoApplyContentConversions(nsIStreamListener* aNextListener,
                                           nsIStreamListener** aNewNextListener,
                                           nsISupports *aCtxt)
{
  *aNewNextListener = nullptr;
  if (!mResponseHead || ! aNextListener) {
    return NS_OK;
  }

  nsCOMPtr<nsIStreamListener> nextListener = aNextListener;

  LOG(("HttpBaseChannel::DoApplyContentConversions [this=%p]\n", this));

  if (!mApplyConversion) {
    LOG(("not applying conversion per mApplyConversion\n"));
    return NS_OK;
  }

  nsAutoCString contentEncoding;
  nsresult rv = mResponseHead->GetHeader(nsHttp::Content_Encoding, contentEncoding);
  if (NS_FAILED(rv) || contentEncoding.IsEmpty())
    return NS_OK;

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

    bool isHTTPS = false;
    mURI->SchemeIs("https", &isHTTPS);
    if (gHttpHandler->IsAcceptableEncoding(val, isHTTPS)) {
      nsCOMPtr<nsIStreamConverterService> serv;
      rv = gHttpHandler->GetStreamConverterService(getter_AddRefs(serv));

      // we won't fail to load the page just because we couldn't load the
      // stream converter service.. carry on..
      if (NS_FAILED(rv)) {
        if (val)
          LOG(("Unknown content encoding '%s', ignoring\n", val));
        continue;
      }

      nsCOMPtr<nsIStreamListener> converter;
      nsAutoCString from(val);
      ToLowerCase(from);
      rv = serv->AsyncConvertData(from.get(),
                                  "uncompressed",
                                  nextListener,
                                  aCtxt,
                                  getter_AddRefs(converter));
      if (NS_FAILED(rv)) {
        LOG(("Unexpected failure of AsyncConvertData %s\n", val));
        return rv;
      }

      LOG(("converter removed '%s' content-encoding\n", val));
      nextListener = converter;
    }
    else {
      if (val)
        LOG(("Unknown content encoding '%s', ignoring\n", val));
    }
  }
  *aNewNextListener = nextListener;
  NS_IF_ADDREF(*aNewNextListener);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentEncodings(nsIUTF8StringEnumerator** aEncodings)
{
  if (!mResponseHead) {
    *aEncodings = nullptr;
    return NS_OK;
  }

  const char *encoding = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
  if (!encoding) {
    *aEncodings = nullptr;
    return NS_OK;
  }
  nsContentEncodings* enumerator = new nsContentEncodings(this, encoding);
  NS_ADDREF(*aEncodings = enumerator);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings <public>
//-----------------------------------------------------------------------------

HttpBaseChannel::nsContentEncodings::nsContentEncodings(nsIHttpChannel* aChannel,
                                                        const char* aEncodingHeader)
  : mEncodingHeader(aEncodingHeader)
  , mChannel(aChannel)
  , mReady(false)
{
  mCurEnd = aEncodingHeader + strlen(aEncodingHeader);
  mCurStart = mCurEnd;
}

HttpBaseChannel::nsContentEncodings::~nsContentEncodings()
{
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings::nsISimpleEnumerator
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::nsContentEncodings::HasMore(bool* aMoreEncodings)
{
  if (mReady) {
    *aMoreEncodings = true;
    return NS_OK;
  }

  nsresult rv = PrepareForNext();
  *aMoreEncodings = NS_SUCCEEDED(rv);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::nsContentEncodings::GetNext(nsACString& aNextEncoding)
{
  aNextEncoding.Truncate();
  if (!mReady) {
    nsresult rv = PrepareForNext();
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  const nsACString & encoding = Substring(mCurStart, mCurEnd);

  nsACString::const_iterator start, end;
  encoding.BeginReading(start);
  encoding.EndReading(end);

  bool haveType = false;
  if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("gzip"), start, end)) {
    aNextEncoding.AssignLiteral(APPLICATION_GZIP);
    haveType = true;
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("compress"), start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_COMPRESS);
      haveType = true;
    }
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("deflate"), start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_ZIP);
      haveType = true;
    }
  }

  // Prepare to fetch the next encoding
  mCurEnd = mCurStart;
  mReady = false;

  if (haveType)
    return NS_OK;

  NS_WARNING("Unknown encoding type");
  return NS_ERROR_FAILURE;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(HttpBaseChannel::nsContentEncodings, nsIUTF8StringEnumerator)

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsContentEncodings <private>
//-----------------------------------------------------------------------------

nsresult
HttpBaseChannel::nsContentEncodings::PrepareForNext(void)
{
  MOZ_ASSERT(mCurStart == mCurEnd, "Indeterminate state");

  // At this point both mCurStart and mCurEnd point to somewhere
  // past the end of the next thing we want to return

  while (mCurEnd != mEncodingHeader) {
    --mCurEnd;
    if (*mCurEnd != ',' && !nsCRT::IsAsciiSpace(*mCurEnd))
      break;
  }
  if (mCurEnd == mEncodingHeader)
    return NS_ERROR_NOT_AVAILABLE; // no more encodings
  ++mCurEnd;

  // At this point mCurEnd points to the first char _after_ the
  // header we want.  Furthermore, mCurEnd - 1 != mEncodingHeader

  mCurStart = mCurEnd - 1;
  while (mCurStart != mEncodingHeader &&
         *mCurStart != ',' && !nsCRT::IsAsciiSpace(*mCurStart))
    --mCurStart;
  if (*mCurStart == ',' || nsCRT::IsAsciiSpace(*mCurStart))
    ++mCurStart; // we stopped because of a weird char, so move up one

  // At this point mCurStart and mCurEnd bracket the encoding string
  // we want.  Check that it's not "identity"
  if (Substring(mCurStart, mCurEnd).Equals("identity",
                                           nsCaseInsensitiveCStringComparator())) {
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
HttpBaseChannel::GetRequestMethod(nsACString& aMethod)
{
  aMethod = mRequestHead.Method();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRequestMethod(const nsACString& aMethod)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  const nsCString& flatMethod = PromiseFlatCString(aMethod);

  // Method names are restricted to valid HTTP tokens.
  if (!nsHttp::IsValidToken(flatMethod))
    return NS_ERROR_INVALID_ARG;

  mRequestHead.SetMethod(flatMethod);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetNetworkInterfaceId(nsACString& aNetworkInterfaceId)
{
  aNetworkInterfaceId = mNetworkInterfaceId;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetNetworkInterfaceId(const nsACString& aNetworkInterfaceId)
{
  ENSURE_CALLED_BEFORE_CONNECT();
  mNetworkInterfaceId = aNetworkInterfaceId;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetReferrer(nsIURI **referrer)
{
  NS_ENSURE_ARG_POINTER(referrer);
  *referrer = mReferrer;
  NS_IF_ADDREF(*referrer);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetReferrer(nsIURI *referrer)
{
  return SetReferrerWithPolicy(referrer, REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE);
}

NS_IMETHODIMP
HttpBaseChannel::GetReferrerPolicy(uint32_t *referrerPolicy)
{
  NS_ENSURE_ARG_POINTER(referrerPolicy);
  *referrerPolicy = mReferrerPolicy;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetReferrerWithPolicy(nsIURI *referrer,
                                       uint32_t referrerPolicy)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  // clear existing referrer, if any
  mReferrer = nullptr;
  mRequestHead.ClearHeader(nsHttp::Referer);
  mReferrerPolicy = REFERRER_POLICY_NO_REFERRER_WHEN_DOWNGRADE;

  if (!referrer) {
    return NS_OK;
  }

  // Don't send referrer at all when the meta referrer setting is "no-referrer"
  if (referrerPolicy == REFERRER_POLICY_NO_REFERRER) {
    mReferrerPolicy = REFERRER_POLICY_NO_REFERRER;
    return NS_OK;
  }

  // 0: never send referer
  // 1: send referer for direct user action
  // 2: always send referer
  uint32_t userReferrerLevel = gHttpHandler->ReferrerLevel();

  // false: use real referrer
  // true: spoof with URI of the current request
  bool userSpoofReferrerSource = gHttpHandler->SpoofReferrerSource();

  // 0: full URI
  // 1: scheme+host+port+path
  // 2: scheme+host+port
  int userReferrerTrimmingPolicy = gHttpHandler->ReferrerTrimmingPolicy();

  // 0: send referer no matter what
  // 1: send referer ONLY when base domains match
  // 2: send referer ONLY when hosts match
  int userReferrerXOriginPolicy = gHttpHandler->ReferrerXOriginPolicy();

  // check referrer blocking pref
  uint32_t referrerLevel;
  if (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI) {
    referrerLevel = 1; // user action
  } else {
    referrerLevel = 2; // inline content
  }
  if (userReferrerLevel < referrerLevel) {
    return NS_OK;
  }

  nsCOMPtr<nsIURI> referrerGrip;
  nsresult rv;
  bool match;

  //
  // Strip off "wyciwyg://123/" from wyciwyg referrers.
  //
  // XXX this really belongs elsewhere since wyciwyg URLs aren't part of necko.
  //   perhaps some sort of generic nsINestedURI could be used.  then, if an URI
  //   fails the whitelist test, then we could check for an inner URI and try
  //   that instead.  though, that might be too automatic.
  //
  rv = referrer->SchemeIs("wyciwyg", &match);
  if (NS_FAILED(rv)) return rv;
  if (match) {
    nsAutoCString path;
    rv = referrer->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    uint32_t pathLength = path.Length();
    if (pathLength <= 2) return NS_ERROR_FAILURE;

    // Path is of the form "//123/http://foo/bar", with a variable number of
    // digits. To figure out where the "real" URL starts, search path for a
    // '/', starting at the third character.
    int32_t slashIndex = path.FindChar('/', 2);
    if (slashIndex == kNotFound) return NS_ERROR_FAILURE;

    // Get charset of the original URI so we can pass it to our fixed up URI.
    nsAutoCString charset;
    referrer->GetOriginCharset(charset);

    // Replace |referrer| with a URI without wyciwyg://123/.
    rv = NS_NewURI(getter_AddRefs(referrerGrip),
                   Substring(path, slashIndex + 1, pathLength - slashIndex - 1),
                   charset.get());
    if (NS_FAILED(rv)) return rv;

    referrer = referrerGrip.get();
  }

  //
  // block referrer if not on our white list...
  //
  static const char *const referrerWhiteList[] = {
    "http",
    "https",
    "ftp",
    nullptr
  };
  match = false;
  const char *const *scheme = referrerWhiteList;
  for (; *scheme && !match; ++scheme) {
    rv = referrer->SchemeIs(*scheme, &match);
    if (NS_FAILED(rv)) return rv;
  }
  if (!match) return NS_OK; // kick out....

  //
  // Handle secure referrals.
  //
  // Support referrals from a secure server if this is a secure site
  // and (optionally) if the host names are the same.
  //
  rv = referrer->SchemeIs("https", &match);
  if (NS_FAILED(rv)) return rv;

  if (match) {
    rv = mURI->SchemeIs("https", &match);
    if (NS_FAILED(rv)) return rv;

    // It's ok to send referrer for https-to-http scenarios if the referrer
    // policy is "unsafe-url", "origin", or "origin-when-cross-origin".
    if (referrerPolicy != REFERRER_POLICY_UNSAFE_URL &&
	referrerPolicy != REFERRER_POLICY_ORIGIN_WHEN_XORIGIN &&
        referrerPolicy != REFERRER_POLICY_ORIGIN) {

      // in other referrer policies, https->http is not allowed...
      if (!match) return NS_OK;

      // ...and https->https is possibly only allowed if the hosts match.
      if (!gHttpHandler->SendSecureXSiteReferrer()) {
        nsAutoCString referrerHost;
        nsAutoCString host;

        rv = referrer->GetAsciiHost(referrerHost);
        if (NS_FAILED(rv)) return rv;

        rv = mURI->GetAsciiHost(host);
        if (NS_FAILED(rv)) return rv;

        // GetAsciiHost returns lowercase hostname.
        if (!referrerHost.Equals(host))
          return NS_OK;
      }
    }
  }

  // for cross-origin-based referrer changes (not just host-based), figure out
  // if the referrer is being sent cross-origin.
  nsCOMPtr<nsIURI> triggeringURI;
  bool isCrossOrigin = true;
  if (mLoadInfo) {
    mLoadInfo->TriggeringPrincipal()->GetURI(getter_AddRefs(triggeringURI));
  }
  if (triggeringURI) {
    if (LOG_ENABLED()) {
      nsAutoCString triggeringURISpec;
      rv = triggeringURI->GetAsciiSpec(triggeringURISpec);
      if (!NS_FAILED(rv)) {
        LOG(("triggeringURI=%s\n", triggeringURISpec.get()));
      }
    }
    nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
    rv = ssm->CheckSameOriginURI(triggeringURI, mURI, false);
    isCrossOrigin = NS_FAILED(rv);
  } else {
    NS_WARNING("no triggering principal available via loadInfo, assuming load is cross-origin");
  }

  nsCOMPtr<nsIURI> clone;
  //
  // we need to clone the referrer, so we can:
  //  (1) modify it
  //  (2) keep a reference to it after returning from this function
  //
  // Use CloneIgnoringRef to strip away any fragment per RFC 2616 section 14.36
  // and Referrer Policy section 6.3.5.
  rv = referrer->CloneIgnoringRef(getter_AddRefs(clone));
  if (NS_FAILED(rv)) return rv;

  nsAutoCString currentHost;
  nsAutoCString referrerHost;

  rv = mURI->GetAsciiHost(currentHost);
  if (NS_FAILED(rv)) return rv;

  rv = clone->GetAsciiHost(referrerHost);
  if (NS_FAILED(rv)) return rv;

  // check policy for sending ref only when hosts match
  if (userReferrerXOriginPolicy == 2 && !currentHost.Equals(referrerHost))
    return NS_OK;

  if (userReferrerXOriginPolicy == 1) {
    nsAutoCString currentDomain = currentHost;
    nsAutoCString referrerDomain = referrerHost;
    uint32_t extraDomains = 0;
    nsCOMPtr<nsIEffectiveTLDService> eTLDService = do_GetService(
      NS_EFFECTIVETLDSERVICE_CONTRACTID);
    if (eTLDService) {
      rv = eTLDService->GetBaseDomain(mURI, extraDomains, currentDomain);
      if (NS_FAILED(rv)) return rv;
      rv = eTLDService->GetBaseDomain(clone, extraDomains, referrerDomain); 
      if (NS_FAILED(rv)) return rv;
    }

    // check policy for sending only when effective top level domain matches.
    // this falls back on using host if eTLDService does not work
    if (!currentDomain.Equals(referrerDomain))
      return NS_OK;
  }

  // send spoofed referrer if desired
  if (userSpoofReferrerSource) {
    nsCOMPtr<nsIURI> mURIclone;
    rv = mURI->CloneIgnoringRef(getter_AddRefs(mURIclone));
    if (NS_FAILED(rv)) return rv;
    clone = mURIclone;
    currentHost = referrerHost;
  }

  // strip away any userpass; we don't want to be giving out passwords ;-)
  // This is required by Referrer Policy stripping algorithm.
  rv = clone->SetUserPass(EmptyCString());
  if (NS_FAILED(rv)) return rv;

  nsAutoCString spec;

  // site-specified referrer trimming may affect the trim level
  // "unsafe-url" behaves like "origin" (send referrer in the same situations) but
  // "unsafe-url" sends the whole referrer and origin removes the path.
  // "origin-when-cross-origin" trims the referrer only when the request is
  // cross-origin.
  if (referrerPolicy == REFERRER_POLICY_ORIGIN ||
      (isCrossOrigin && referrerPolicy == REFERRER_POLICY_ORIGIN_WHEN_XORIGIN)) {
    userReferrerTrimmingPolicy = 2;
  }

  // check how much referer to send
  switch (userReferrerTrimmingPolicy) {

  case 1: {
    // scheme+host+port+path
    nsAutoCString prepath, path;
    rv = clone->GetPrePath(prepath);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIURL> url(do_QueryInterface(clone));
    if (!url) {
      // if this isn't a url, play it safe
      // and just send the prepath
      spec = prepath;
      break;
    }
    rv = url->GetFilePath(path);
    if (NS_FAILED(rv)) return rv;
    spec = prepath + path;
    break;
  }
  case 2:
    // scheme+host+port
    rv = clone->GetPrePath(spec);
    if (NS_FAILED(rv)) return rv;
    break;

  default:
    // full URI
    rv = clone->GetAsciiSpec(spec);
    if (NS_FAILED(rv)) return rv;
    break;
  }

  // finally, remember the referrer URI and set the Referer header.
  rv = SetRequestHeader(NS_LITERAL_CSTRING("Referer"), spec, false);
  if (NS_FAILED(rv)) return rv;

  mReferrer = clone;
  mReferrerPolicy = referrerPolicy;
  return NS_OK;
}

// Return the channel's proxy URI, or if it doesn't exist, the
// channel's main URI.
NS_IMETHODIMP
HttpBaseChannel::GetProxyURI(nsIURI **aOut)
{
  NS_ENSURE_ARG_POINTER(aOut);
  nsCOMPtr<nsIURI> result(mProxyURI);
  result.forget(aOut);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestHeader(const nsACString& aHeader,
                                  nsACString& aValue)
{
  aValue.Truncate();

  // XXX might be better to search the header list directly instead of
  // hitting the http atom hash table.
  nsHttpAtom atom = nsHttp::ResolveAtom(aHeader);
  if (!atom)
    return NS_ERROR_NOT_AVAILABLE;

  return mRequestHead.GetHeader(atom, aValue);
}

NS_IMETHODIMP
HttpBaseChannel::SetRequestHeader(const nsACString& aHeader,
                                  const nsACString& aValue,
                                  bool aMerge)
{
  const nsCString &flatHeader = PromiseFlatCString(aHeader);
  const nsCString &flatValue  = PromiseFlatCString(aValue);

  LOG(("HttpBaseChannel::SetRequestHeader [this=%p header=\"%s\" value=\"%s\" merge=%u]\n",
      this, flatHeader.get(), flatValue.get(), aMerge));

  // Verify header names are valid HTTP tokens and header values are reasonably
  // close to whats allowed in RFC 2616.
  if (!nsHttp::IsValidToken(flatHeader) ||
      !nsHttp::IsReasonableHeaderValue(flatValue)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsHttpAtom atom = nsHttp::ResolveAtom(flatHeader.get());
  if (!atom) {
    NS_WARNING("failed to resolve atom");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mRequestHead.SetHeader(atom, flatValue, aMerge);
}

NS_IMETHODIMP
HttpBaseChannel::SetEmptyRequestHeader(const nsACString& aHeader)
{
  const nsCString &flatHeader = PromiseFlatCString(aHeader);

  LOG(("HttpBaseChannel::SetEmptyRequestHeader [this=%p header=\"%s\"]\n",
      this, flatHeader.get()));

  // Verify header names are valid HTTP tokens and header values are reasonably
  // close to whats allowed in RFC 2616.
  if (!nsHttp::IsValidToken(flatHeader)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsHttpAtom atom = nsHttp::ResolveAtom(flatHeader.get());
  if (!atom) {
    NS_WARNING("failed to resolve atom");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mRequestHead.SetEmptyHeader(atom);
}

NS_IMETHODIMP
HttpBaseChannel::VisitRequestHeaders(nsIHttpHeaderVisitor *visitor)
{
  return mRequestHead.Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
HttpBaseChannel::VisitNonDefaultRequestHeaders(nsIHttpHeaderVisitor *visitor)
{
  return mRequestHead.Headers().VisitHeaders(
      visitor, nsHttpHeaderArray::eFilterSkipDefault);
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseHeader(const nsACString &header, nsACString &value)
{
  value.Truncate();

  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  nsHttpAtom atom = nsHttp::ResolveAtom(header);
  if (!atom)
    return NS_ERROR_NOT_AVAILABLE;

  return mResponseHead->GetHeader(atom, value);
}

NS_IMETHODIMP
HttpBaseChannel::SetResponseHeader(const nsACString& header,
                                   const nsACString& value,
                                   bool merge)
{
  LOG(("HttpBaseChannel::SetResponseHeader [this=%p header=\"%s\" value=\"%s\" merge=%u]\n",
      this, PromiseFlatCString(header).get(), PromiseFlatCString(value).get(), merge));

  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  nsHttpAtom atom = nsHttp::ResolveAtom(header);
  if (!atom)
    return NS_ERROR_NOT_AVAILABLE;

  // these response headers must not be changed
  if (atom == nsHttp::Content_Type ||
      atom == nsHttp::Content_Length ||
      atom == nsHttp::Content_Encoding ||
      atom == nsHttp::Trailer ||
      atom == nsHttp::Transfer_Encoding)
    return NS_ERROR_ILLEGAL_VALUE;

  mResponseHeadersModified = true;

  return mResponseHead->SetHeader(atom, value, merge);
}

NS_IMETHODIMP
HttpBaseChannel::VisitResponseHeaders(nsIHttpHeaderVisitor *visitor)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  return mResponseHead->Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowPipelining(bool *value)
{
  NS_ENSURE_ARG_POINTER(value);
  *value = mAllowPipelining;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowPipelining(bool value)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  mAllowPipelining = value;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowSTS(bool *value)
{
  NS_ENSURE_ARG_POINTER(value);
  *value = mAllowSTS;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowSTS(bool value)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  mAllowSTS = value;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectionLimit(uint32_t *value)
{
  NS_ENSURE_ARG_POINTER(value);
  *value = mRedirectionLimit;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectionLimit(uint32_t value)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  mRedirectionLimit = std::min<uint32_t>(value, 0xff);
  return NS_OK;
}

nsresult
HttpBaseChannel::OverrideSecurityInfo(nsISupports* aSecurityInfo)
{
  MOZ_ASSERT(!mSecurityInfo,
             "This can only be called when we don't have a security info object already");
  MOZ_RELEASE_ASSERT(aSecurityInfo,
                     "This can only be called with a valid security info object");
  MOZ_ASSERT(mResponseCouldBeSynthesized,
             "This can only be called on channels that can be intercepted");
  if (mSecurityInfo) {
    LOG(("HttpBaseChannel::OverrideSecurityInfo mSecurityInfo is null! "
         "[this=%p]\n", this));
    return NS_ERROR_UNEXPECTED;
  }
  if (!mResponseCouldBeSynthesized) {
    LOG(("HttpBaseChannel::OverrideSecurityInfo channel cannot be intercepted! "
         "[this=%p]\n", this));
    return NS_ERROR_UNEXPECTED;
  }

  mSecurityInfo = aSecurityInfo;
  return NS_OK;
}

nsresult
HttpBaseChannel::OverrideURI(nsIURI* aRedirectedURI)
{
  MOZ_ASSERT(mLoadFlags & LOAD_REPLACE,
             "This can only happen if the LOAD_REPLACE flag is set");
  MOZ_ASSERT(ShouldIntercept(),
             "This can only be called on channels that can be intercepted");
  if (!(mLoadFlags & LOAD_REPLACE)) {
    LOG(("HttpBaseChannel::OverrideURI LOAD_REPLACE flag not set! [this=%p]\n",
         this));
    return NS_ERROR_UNEXPECTED;
  }
  if (!mResponseCouldBeSynthesized) {
    LOG(("HttpBaseChannel::OverrideURI channel cannot be intercepted! "
         "[this=%p]\n", this));
    return NS_ERROR_UNEXPECTED;
  }

  mURI = aRedirectedURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoStoreResponse(bool *value)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoStore();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoCacheResponse(bool *value)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoCache();
  if (!*value)
    *value = mResponseHead->ExpiresInPast();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsPrivateResponse(bool *value)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->Private();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStatus(uint32_t *aValue)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *aValue = mResponseHead->Status();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStatusText(nsACString& aValue)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  aValue = mResponseHead->StatusText();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestSucceeded(bool *aValue)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  uint32_t status = mResponseHead->Status();
  *aValue = (status / 100 == 2);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::RedirectTo(nsIURI *newURI)
{
  // We can only redirect unopened channels
  ENSURE_CALLED_BEFORE_CONNECT();

  // The redirect is stored internally for use in AsyncOpen
  mAPIRedirectToURI = newURI;

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetSchedulingContextID(nsID *aSCID)
{
  NS_ENSURE_ARG_POINTER(aSCID);
  *aSCID = mSchedulingContextID;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetSchedulingContextID(const nsID aSCID)
{
  mSchedulingContextID = aSCID;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetTopWindowURI(nsIURI **aTopWindowURI)
{
  nsresult rv = NS_OK;
  nsCOMPtr<mozIThirdPartyUtil> util;
  // Only compute the top window URI once. In e10s, this must be computed in the
  // child. The parent gets the top window URI through HttpChannelOpenArgs.
  if (!mTopWindowURI) {
    util = do_GetService(THIRDPARTYUTIL_CONTRACTID);
    if (!util) {
      return NS_ERROR_NOT_AVAILABLE;
    }
    nsCOMPtr<nsIDOMWindow> win;
    nsresult rv = util->GetTopWindowForChannel(this, getter_AddRefs(win));
    if (NS_SUCCEEDED(rv)) {
      rv = util->GetURIFromWindow(win, getter_AddRefs(mTopWindowURI));
#if DEBUG
      if (mTopWindowURI) {
        nsCString spec;
        rv = mTopWindowURI->GetSpec(spec);
        LOG(("HttpChannelBase::Setting topwindow URI spec %s [this=%p]\n",
             spec.get(), this));
      }
#endif
    }
  }
  NS_IF_ADDREF(*aTopWindowURI = mTopWindowURI);
  return rv;
}

NS_IMETHODIMP
HttpBaseChannel::GetDocumentURI(nsIURI **aDocumentURI)
{
  NS_ENSURE_ARG_POINTER(aDocumentURI);
  *aDocumentURI = mDocumentURI;
  NS_IF_ADDREF(*aDocumentURI);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetDocumentURI(nsIURI *aDocumentURI)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  mDocumentURI = aDocumentURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestVersion(uint32_t *major, uint32_t *minor)
{
  nsHttpVersion version = mRequestHead.Version();

  if (major) { *major = version / 10; }
  if (minor) { *minor = version % 10; }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseVersion(uint32_t *major, uint32_t *minor)
{
  if (!mResponseHead)
  {
    *major = *minor = 0; // we should at least be kind about it
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsHttpVersion version = mResponseHead->Version();

  if (major) { *major = version / 10; }
  if (minor) { *minor = version % 10; }

  return NS_OK;
}

namespace {

class CookieNotifierRunnable : public nsRunnable
{
public:
  CookieNotifierRunnable(HttpBaseChannel* aChannel, char const * aCookie)
    : mChannel(aChannel), mCookie(aCookie)
  { }

  NS_IMETHOD Run()
  {
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(static_cast<nsIChannel*>(mChannel.get()),
                           "http-on-response-set-cookie",
                           mCookie.get());
    }
    return NS_OK;
  }

private:
  nsRefPtr<HttpBaseChannel> mChannel;
  NS_ConvertASCIItoUTF16 mCookie;
};

} // namespace

NS_IMETHODIMP
HttpBaseChannel::SetCookie(const char *aCookieHeader)
{
  if (mLoadFlags & LOAD_ANONYMOUS)
    return NS_OK;

  // empty header isn't an error
  if (!(aCookieHeader && *aCookieHeader))
    return NS_OK;

  nsICookieService *cs = gHttpHandler->GetCookieService();
  NS_ENSURE_TRUE(cs, NS_ERROR_FAILURE);

  nsresult rv =
    cs->SetCookieStringFromHttp(mURI, nullptr, nullptr, aCookieHeader,
                                mResponseHead->PeekHeader(nsHttp::Date), this);
  if (NS_SUCCEEDED(rv)) {
    nsRefPtr<CookieNotifierRunnable> r =
      new CookieNotifierRunnable(this, aCookieHeader);
    NS_DispatchToMainThread(r);
  }
  return rv;
}

NS_IMETHODIMP
HttpBaseChannel::GetThirdPartyFlags(uint32_t  *aFlags)
{
  *aFlags = mThirdPartyFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetThirdPartyFlags(uint32_t aFlags)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mThirdPartyFlags = aFlags;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetForceAllowThirdPartyCookie(bool *aForce)
{
  *aForce = !!(mThirdPartyFlags & nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetForceAllowThirdPartyCookie(bool aForce)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  if (aForce)
    mThirdPartyFlags |= nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW;
  else
    mThirdPartyFlags &= ~nsIHttpChannelInternal::THIRD_PARTY_FORCE_ALLOW;

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCanceled(bool *aCanceled)
{
  *aCanceled = mCanceled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetChannelIsForDownload(bool *aChannelIsForDownload)
{
  *aChannelIsForDownload = mChannelIsForDownload;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetChannelIsForDownload(bool aChannelIsForDownload)
{
  mChannelIsForDownload = aChannelIsForDownload;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCacheKeysRedirectChain(nsTArray<nsCString> *cacheKeys)
{
  mRedirectedCachekeys = cacheKeys;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLocalAddress(nsACString& addr)
{
  if (mSelfAddr.raw.family == PR_AF_UNSPEC)
    return NS_ERROR_NOT_AVAILABLE;

  addr.SetCapacity(kIPv6CStrBufSize);
  NetAddrToString(&mSelfAddr, addr.BeginWriting(), kIPv6CStrBufSize);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::TakeAllSecurityMessages(
    nsCOMArray<nsISecurityConsoleMessage> &aMessages)
{
  aMessages.Clear();
  aMessages.SwapElements(mSecurityConsoleMessages);
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
nsresult
HttpBaseChannel::AddSecurityMessage(const nsAString &aMessageTag,
    const nsAString &aMessageCategory)
{
  nsresult rv;
  nsCOMPtr<nsISecurityConsoleMessage> message =
    do_CreateInstance(NS_SECURITY_CONSOLE_MESSAGE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  message->SetTag(aMessageTag);
  message->SetCategory(aMessageCategory);
  mSecurityConsoleMessages.AppendElement(message);

  nsCOMPtr<nsIConsoleService> console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!console) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsILoadInfo> loadInfo;
  GetLoadInfo(getter_AddRefs(loadInfo));
  if (!loadInfo) {
    return NS_ERROR_FAILURE;
  }

  uint32_t innerWindowID = loadInfo->GetInnerWindowID();

  nsXPIDLString errorText;
  rv = nsContentUtils::GetLocalizedString(
          nsContentUtils::eSECURITY_PROPERTIES,
          NS_ConvertUTF16toUTF8(aMessageTag).get(),
          errorText);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoCString spec;
  if (mURI) {
    mURI->GetSpec(spec);
  }

  nsCOMPtr<nsIScriptError> error(do_CreateInstance(NS_SCRIPTERROR_CONTRACTID));
  error->InitWithWindowID(errorText, NS_ConvertUTF8toUTF16(spec),
                          EmptyString(), 0, 0, nsIScriptError::warningFlag,
                          NS_ConvertUTF16toUTF8(aMessageCategory),
                          innerWindowID);
  console->LogMessage(error);

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLocalPort(int32_t* port)
{
  NS_ENSURE_ARG_POINTER(port);

  if (mSelfAddr.raw.family == PR_AF_INET) {
    *port = (int32_t)ntohs(mSelfAddr.inet.port);
  }
  else if (mSelfAddr.raw.family == PR_AF_INET6) {
    *port = (int32_t)ntohs(mSelfAddr.inet6.port);
  }
  else
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRemoteAddress(nsACString& addr)
{
  if (mPeerAddr.raw.family == PR_AF_UNSPEC)
    return NS_ERROR_NOT_AVAILABLE;

  addr.SetCapacity(kIPv6CStrBufSize);
  NetAddrToString(&mPeerAddr, addr.BeginWriting(), kIPv6CStrBufSize);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRemotePort(int32_t* port)
{
  NS_ENSURE_ARG_POINTER(port);

  if (mPeerAddr.raw.family == PR_AF_INET) {
    *port = (int32_t)ntohs(mPeerAddr.inet.port);
  }
  else if (mPeerAddr.raw.family == PR_AF_INET6) {
    *port = (int32_t)ntohs(mPeerAddr.inet6.port);
  }
  else
    return NS_ERROR_NOT_AVAILABLE;

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::HTTPUpgrade(const nsACString &aProtocolName,
                             nsIHttpUpgradeListener *aListener)
{
    NS_ENSURE_ARG(!aProtocolName.IsEmpty());
    NS_ENSURE_ARG_POINTER(aListener);

    mUpgradeProtocol = aProtocolName;
    mUpgradeProtocolCallback = aListener;
    return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowSpdy(bool *aAllowSpdy)
{
  NS_ENSURE_ARG_POINTER(aAllowSpdy);

  *aAllowSpdy = mAllowSpdy;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowSpdy(bool aAllowSpdy)
{
  mAllowSpdy = aAllowSpdy;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllowAltSvc(bool *aAllowAltSvc)
{
  NS_ENSURE_ARG_POINTER(aAllowAltSvc);

  *aAllowAltSvc = mAllowAltSvc;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowAltSvc(bool aAllowAltSvc)
{
  mAllowAltSvc = aAllowAltSvc;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetApiRedirectToURI(nsIURI ** aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  NS_IF_ADDREF(*aResult = mAPIRedirectToURI);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseTimeoutEnabled(bool *aEnable)
{
  if (NS_WARN_IF(!aEnable)) {
    return NS_ERROR_NULL_POINTER;
  }
  *aEnable = mResponseTimeoutEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetResponseTimeoutEnabled(bool aEnable)
{
  mResponseTimeoutEnabled = aEnable;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetInitialRwin(uint32_t *aRwin)
{
  if (NS_WARN_IF(!aRwin)) {
    return NS_ERROR_NULL_POINTER;
  }
  *aRwin = mInitialRwin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetInitialRwin(uint32_t aRwin)
{
  ENSURE_CALLED_BEFORE_CONNECT();
  mInitialRwin = aRwin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::ForcePending(bool aForcePending)
{
  mForcePending = aForcePending;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLastModifiedTime(PRTime* lastModifiedTime)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  uint32_t lastMod;
  mResponseHead->GetLastModifiedValue(&lastMod);
  *lastModifiedTime = lastMod;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::ForceNoIntercept()
{
  mForceNoIntercept = true;
  mResponseCouldBeSynthesized = false;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCorsIncludeCredentials(bool* aInclude)
{
  *aInclude = mCorsIncludeCredentials;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCorsIncludeCredentials(bool aInclude)
{
  mCorsIncludeCredentials = aInclude;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCorsMode(uint32_t* aMode)
{
  *aMode = mCorsMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetCorsMode(uint32_t aMode)
{
  mCorsMode = aMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectMode(uint32_t* aMode)
{
  *aMode = mRedirectMode;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectMode(uint32_t aMode)
{
  mRedirectMode = aMode;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetPriority(int32_t *value)
{
  *value = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::AdjustPriority(int32_t delta)
{
  return SetPriority(mPriority + delta);
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIResumableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetEntityID(nsACString& aEntityID)
{
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
    const char* acceptRanges =
        mResponseHead->PeekHeader(nsHttp::Accept_Ranges);
    if (acceptRanges &&
        !nsHttp::FindToken(acceptRanges, "bytes", HTTP_HEADER_VALUE_SEPS)) {
      return NS_ERROR_NOT_RESUMABLE;
    }

    size = mResponseHead->TotalEntitySize();
    const char* cLastMod = mResponseHead->PeekHeader(nsHttp::Last_Modified);
    if (cLastMod)
      lastmod = cLastMod;
    const char* cEtag = mResponseHead->PeekHeader(nsHttp::ETag);
    if (cEtag)
      etag = cEtag;
  }
  nsCString entityID;
  NS_EscapeURL(etag.BeginReading(), etag.Length(), esc_AlwaysCopy |
               esc_FileBaseName | esc_Forced, entityID);
  entityID.Append('/');
  entityID.AppendInt(int64_t(size));
  entityID.Append('/');
  entityID.Append(lastmod);
  // NOTE: Appending lastmod as the last part avoids having to escape it

  aEntityID = entityID;

  return NS_OK;
}

nsIPrincipal *
HttpBaseChannel::GetURIPrincipal()
{
  if (mPrincipal) {
      return mPrincipal;
  }

  nsIScriptSecurityManager *securityManager =
      nsContentUtils::GetSecurityManager();

  if (!securityManager) {
      LOG(("HttpBaseChannel::GetURIPrincipal: No security manager [this=%p]",
           this));
      return nullptr;
  }

  securityManager->GetChannelURIPrincipal(this, getter_AddRefs(mPrincipal));
  if (!mPrincipal) {
      LOG(("HttpBaseChannel::GetURIPrincipal: No channel principal [this=%p]",
           this));
      return nullptr;
  }

  return mPrincipal;
}

bool
HttpBaseChannel::IsNavigation()
{
  return mLoadFlags & LOAD_DOCUMENT_URI;
}

bool
HttpBaseChannel::ShouldIntercept()
{
  nsCOMPtr<nsINetworkInterceptController> controller;
  GetCallback(controller);
  bool shouldIntercept = false;
  if (controller && !mForceNoIntercept) {
    nsresult rv = controller->ShouldPrepareForIntercept(mURI,
                                                        IsNavigation(),
                                                        &shouldIntercept);
    if (NS_FAILED(rv)) {
      return false;
    }
  }
  return shouldIntercept;
}

//-----------------------------------------------------------------------------
// nsHttpChannel::nsITraceableChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetNewListener(nsIStreamListener *aListener, nsIStreamListener **_retval)
{
  if (!mTracingEnabled)
    return NS_ERROR_FAILURE;

  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIStreamListener> wrapper = new nsStreamListenerWrapper(mListener);

  wrapper.forget(_retval);
  mListener = aListener;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel helpers
//-----------------------------------------------------------------------------

void
HttpBaseChannel::ReleaseListeners()
{
  MOZ_ASSERT(NS_IsMainThread(), "Should only be called on the main thread.");
  
  mListener = nullptr;
  mListenerContext = nullptr;
  mCallbacks = nullptr;
  mProgressSink = nullptr;
}

void
HttpBaseChannel::DoNotifyListener()
{
  if (mListener) {
    MOZ_ASSERT(!mOnStartRequestCalled,
               "We should not call OnStartRequest twice");

    nsCOMPtr<nsIStreamListener> listener = mListener;
    listener->OnStartRequest(this, mListenerContext);

    mOnStartRequestCalled = true;
  }

  // Make sure mIsPending is set to false. At this moment we are done from
  // the point of view of our consumer and we have to report our self
  // as not-pending.
  mIsPending = false;

  if (mListener) {
    nsCOMPtr<nsIStreamListener> listener = mListener;
    listener->OnStopRequest(this, mListenerContext, mStatus);
  }

  // We have to make sure to drop the references to listeners and callbacks
  // no longer  needed
  ReleaseListeners();

  DoNotifyListenerCleanup();
}

void
HttpBaseChannel::AddCookiesToRequest()
{
  if (mLoadFlags & LOAD_ANONYMOUS) {
    return;
  }

  bool useCookieService =
    (XRE_IsParentProcess());
  nsXPIDLCString cookie;
  if (useCookieService) {
    nsICookieService *cs = gHttpHandler->GetCookieService();
    if (cs) {
      cs->GetCookieStringFromHttp(mURI,
                                  nullptr,
                                  this, getter_Copies(cookie));
    }

    if (cookie.IsEmpty()) {
      cookie = mUserSetCookieHeader;
    }
    else if (!mUserSetCookieHeader.IsEmpty()) {
      cookie.AppendLiteral("; ");
      cookie.Append(mUserSetCookieHeader);
    }
  }
  else {
    cookie = mUserSetCookieHeader;
  }

  // If we are in the child process, we want the parent seeing any
  // cookie headers that might have been set by SetRequestHeader()
  SetRequestHeader(nsDependentCString(nsHttp::Cookie), cookie, false);
}

static PLDHashOperator
CopyProperties(const nsAString& aKey, nsIVariant *aData, void *aClosure)
{
  nsIWritablePropertyBag* bag = static_cast<nsIWritablePropertyBag*>
                                           (aClosure);
  bag->SetProperty(aKey, aData);
  return PL_DHASH_NEXT;
}

bool
HttpBaseChannel::ShouldRewriteRedirectToGET(uint32_t httpStatus,
                                            nsHttpRequestHead::ParsedMethodType method)
{
  // for 301 and 302, only rewrite POST
  if (httpStatus == 301 || httpStatus == 302)
    return method == nsHttpRequestHead::kMethod_Post;

  // rewrite for 303 unless it was HEAD
  if (httpStatus == 303)
    return method != nsHttpRequestHead::kMethod_Head;

  // otherwise, such as for 307, do not rewrite
  return false;
}

nsresult
HttpBaseChannel::SetupReplacementChannel(nsIURI       *newURI,
                                         nsIChannel   *newChannel,
                                         bool          preserveMethod)
{
  LOG(("HttpBaseChannel::SetupReplacementChannel "
     "[this=%p newChannel=%p preserveMethod=%d]",
     this, newChannel, preserveMethod));
  uint32_t newLoadFlags = mLoadFlags | LOAD_REPLACE;
  // if the original channel was using SSL and this channel is not using
  // SSL, then no need to inhibit persistent caching.  however, if the
  // original channel was not using SSL and has INHIBIT_PERSISTENT_CACHING
  // set, then allow the flag to apply to the redirected channel as well.
  // since we force set INHIBIT_PERSISTENT_CACHING on all HTTPS channels,
  // we only need to check if the original channel was using SSL.
  bool usingSSL = false;
  nsresult rv = mURI->SchemeIs("https", &usingSSL);
  if (NS_SUCCEEDED(rv) && usingSSL)
    newLoadFlags &= ~INHIBIT_PERSISTENT_CACHING;

  // Do not pass along LOAD_CHECK_OFFLINE_CACHE
  newLoadFlags &= ~nsICachingChannel::LOAD_CHECK_OFFLINE_CACHE;

  newChannel->SetLoadGroup(mLoadGroup);
  newChannel->SetNotificationCallbacks(mCallbacks);
  newChannel->SetLoadFlags(newLoadFlags);

  // Try to preserve the privacy bit if it has been overridden
  if (mPrivateBrowsingOverriden) {
    nsCOMPtr<nsIPrivateBrowsingChannel> newPBChannel =
      do_QueryInterface(newChannel);
    if (newPBChannel) {
      newPBChannel->SetPrivate(mPrivateBrowsing);
    }
  }

  // Propagate our loadinfo if needed.
  newChannel->SetLoadInfo(mLoadInfo);

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
  if (!httpChannel)
    return NS_OK; // no other options to set

  // Preserve the CORS preflight information.
  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(newChannel);
  if (mRequireCORSPreflight && httpInternal) {
    rv = httpInternal->SetCorsPreflightParameters(mUnsafeHeaders, mWithCredentials,
                                                  mPreflightPrincipal);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (preserveMethod) {
    nsCOMPtr<nsIUploadChannel> uploadChannel =
      do_QueryInterface(httpChannel);
    nsCOMPtr<nsIUploadChannel2> uploadChannel2 =
      do_QueryInterface(httpChannel);
    if (mUploadStream && (uploadChannel2 || uploadChannel)) {
      // rewind upload stream
      nsCOMPtr<nsISeekableStream> seekable = do_QueryInterface(mUploadStream);
      if (seekable)
        seekable->Seek(nsISeekableStream::NS_SEEK_SET, 0);

      // replicate original call to SetUploadStream...
      if (uploadChannel2) {
        const char *ctype = mRequestHead.PeekHeader(nsHttp::Content_Type);
        if (!ctype)
          ctype = "";
        const char *clen  = mRequestHead.PeekHeader(nsHttp::Content_Length);
        int64_t len = clen ? nsCRT::atoll(clen) : -1;
        uploadChannel2->ExplicitSetUploadStream(
                                  mUploadStream, nsDependentCString(ctype), len,
                                  mRequestHead.Method(),
                                  mUploadStreamHasHeaders);
      } else {
        if (mUploadStreamHasHeaders) {
          uploadChannel->SetUploadStream(mUploadStream, EmptyCString(),
                           -1);
        } else {
          const char *ctype =
            mRequestHead.PeekHeader(nsHttp::Content_Type);
          const char *clen =
            mRequestHead.PeekHeader(nsHttp::Content_Length);
          if (!ctype) {
            ctype = "application/octet-stream";
          }
          if (clen) {
            uploadChannel->SetUploadStream(mUploadStream,
                                           nsDependentCString(ctype),
                                           nsCRT::atoll(clen));
          }
        }
      }
    }
    // since preserveMethod is true, we need to ensure that the appropriate
    // request method gets set on the channel, regardless of whether or not
    // we set the upload stream above. This means SetRequestMethod() will
    // be called twice if ExplicitSetUploadStream() gets called above.

    httpChannel->SetRequestMethod(mRequestHead.Method());
  }
  // convey the referrer if one was used for this channel to the next one
  if (mReferrer)
    httpChannel->SetReferrerWithPolicy(mReferrer, mReferrerPolicy);
  // convey the mAllowPipelining and mAllowSTS flags
  httpChannel->SetAllowPipelining(mAllowPipelining);
  httpChannel->SetAllowSTS(mAllowSTS);
  // convey the new redirection limit
  httpChannel->SetRedirectionLimit(mRedirectionLimit - 1);

  // convey the Accept header value
  {
    nsAutoCString oldAcceptValue;
    nsresult hasHeader = mRequestHead.GetHeader(nsHttp::Accept, oldAcceptValue);
    if (NS_SUCCEEDED(hasHeader)) {
      httpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Accept"),
                                    oldAcceptValue,
                                    false);
    }
  }

  if (httpInternal) {
    // Convey third party cookie and spdy flags.
    httpInternal->SetThirdPartyFlags(mThirdPartyFlags);
    httpInternal->SetAllowSpdy(mAllowSpdy);
    httpInternal->SetAllowAltSvc(mAllowAltSvc);

    // update the DocumentURI indicator since we are being redirected.
    // if this was a top-level document channel, then the new channel
    // should have its mDocumentURI point to newURI; otherwise, we
    // just need to pass along our mDocumentURI to the new channel.
    if (newURI && (mURI == mDocumentURI))
      httpInternal->SetDocumentURI(newURI);
    else
      httpInternal->SetDocumentURI(mDocumentURI);

    // if there is a chain of keys for redirect-responses we transfer it to
    // the new channel (see bug #561276)
    if (mRedirectedCachekeys) {
        LOG(("HttpBaseChannel::SetupReplacementChannel "
             "[this=%p] transferring chain of redirect cache-keys", this));
        httpInternal->SetCacheKeysRedirectChain(mRedirectedCachekeys.forget());
    }

    // Preserve any skip-serviceworker-flag.
    if (mForceNoIntercept) {
      httpInternal->ForceNoIntercept();
    }

    // Preserve CORS mode flag.
    httpInternal->SetCorsMode(mCorsMode);

    // Preserve Redirect mode flag.
    httpInternal->SetRedirectMode(mRedirectMode);
  }

  // transfer application cache information
  nsCOMPtr<nsIApplicationCacheChannel> appCacheChannel =
    do_QueryInterface(newChannel);
  if (appCacheChannel) {
    appCacheChannel->SetApplicationCache(mApplicationCache);
    appCacheChannel->SetInheritApplicationCache(mInheritApplicationCache);
    // We purposely avoid transfering mChooseApplicationCache.
  }

  // transfer any properties
  nsCOMPtr<nsIWritablePropertyBag> bag(do_QueryInterface(newChannel));
  if (bag)
    mPropertyHash.EnumerateRead(CopyProperties, bag.get());

  // Transfer the timing data (if we are dealing with an nsITimedChannel).
  nsCOMPtr<nsITimedChannel> newTimedChannel(do_QueryInterface(newChannel));
  nsCOMPtr<nsITimedChannel> oldTimedChannel(
      do_QueryInterface(static_cast<nsIHttpChannel*>(this)));
  if (oldTimedChannel && newTimedChannel) {
    newTimedChannel->SetTimingEnabled(mTimingEnabled);
    newTimedChannel->SetRedirectCount(mRedirectCount + 1);

    // If the RedirectStart is null, we will use the AsyncOpen value of the
    // previous channel (this is the first redirect in the redirects chain).
    if (mRedirectStartTimeStamp.IsNull()) {
      TimeStamp asyncOpen;
      oldTimedChannel->GetAsyncOpen(&asyncOpen);
      newTimedChannel->SetRedirectStart(asyncOpen);
    }
    else {
      newTimedChannel->SetRedirectStart(mRedirectStartTimeStamp);
    }

    // The RedirectEnd timestamp is equal to the previous channel response end.
    TimeStamp prevResponseEnd;
    oldTimedChannel->GetResponseEnd(&prevResponseEnd);
    newTimedChannel->SetRedirectEnd(prevResponseEnd);

    nsAutoString initiatorType;
    oldTimedChannel->GetInitiatorType(initiatorType);
    newTimedChannel->SetInitiatorType(initiatorType);

    // Check whether or not this was a cross-domain redirect.
    newTimedChannel->SetAllRedirectsSameOrigin(
        mAllRedirectsSameOrigin && SameOriginWithOriginalUri(newURI));

    // Execute the timing allow check to determine whether
    // to report the redirect timing info
    nsCOMPtr<nsILoadInfo> loadInfo;
    GetLoadInfo(getter_AddRefs(loadInfo));
    if (loadInfo) {
      nsCOMPtr<nsIPrincipal> principal = loadInfo->LoadingPrincipal();
      newTimedChannel->SetAllRedirectsPassTimingAllowCheck(
        mAllRedirectsPassTimingAllowCheck &&
        oldTimedChannel->TimingAllowCheck(principal));
    }
  }

  // This channel has been redirected. Don't report timing info.
  mTimingEnabled = false;
  return NS_OK;
}

// Redirect Tracking
bool
HttpBaseChannel::SameOriginWithOriginalUri(nsIURI *aURI)
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsresult rv = ssm->CheckSameOriginURI(aURI, mOriginalURI, false);
  return (NS_SUCCEEDED(rv));
}



//-----------------------------------------------------------------------------
// HttpBaseChannel::nsITimedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::SetTimingEnabled(bool enabled) {
  mTimingEnabled = enabled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetTimingEnabled(bool* _retval) {
  *_retval = mTimingEnabled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetChannelCreation(TimeStamp* _retval) {
  *_retval = mChannelCreationTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAsyncOpen(TimeStamp* _retval) {
  *_retval = mAsyncOpenTime;
  return NS_OK;
}

/**
 * @return the number of redirects. There is no check for cross-domain
 * redirects. This check must be done by the consumers.
 */
NS_IMETHODIMP
HttpBaseChannel::GetRedirectCount(uint16_t *aRedirectCount)
{
  *aRedirectCount = mRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectCount(uint16_t aRedirectCount)
{
  mRedirectCount = aRedirectCount;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectStart(TimeStamp* _retval)
{
  *_retval = mRedirectStartTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectStart(TimeStamp aRedirectStart)
{
  mRedirectStartTimeStamp = aRedirectStart;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectEnd(TimeStamp* _retval)
{
  *_retval = mRedirectEndTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectEnd(TimeStamp aRedirectEnd)
{
  mRedirectEndTimeStamp = aRedirectEnd;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllRedirectsSameOrigin(bool *aAllRedirectsSameOrigin)
{
  *aAllRedirectsSameOrigin = mAllRedirectsSameOrigin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllRedirectsSameOrigin(bool aAllRedirectsSameOrigin)
{
  mAllRedirectsSameOrigin = aAllRedirectsSameOrigin;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetAllRedirectsPassTimingAllowCheck(bool *aPassesCheck)
{
  *aPassesCheck = mAllRedirectsPassTimingAllowCheck;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllRedirectsPassTimingAllowCheck(bool aPassesCheck)
{
  mAllRedirectsPassTimingAllowCheck = aPassesCheck;
  return NS_OK;
}

// http://www.w3.org/TR/resource-timing/#timing-allow-check
NS_IMETHODIMP
HttpBaseChannel::TimingAllowCheck(nsIPrincipal *aOrigin, bool *_retval)
{
  nsIScriptSecurityManager* ssm = nsContentUtils::GetSecurityManager();
  nsCOMPtr<nsIPrincipal> resourcePrincipal;
  nsresult rv = ssm->GetChannelURIPrincipal(this, getter_AddRefs(resourcePrincipal));
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
  rv = GetResponseHeader(NS_LITERAL_CSTRING("Timing-Allow-Origin"), headerValue);
  if (NS_FAILED(rv)) {
    *_retval = false;
    return NS_OK;
  }

  if (headerValue == "*") {
    *_retval = true;
    return NS_OK;
  }

  nsAutoCString origin;
  nsContentUtils::GetASCIIOrigin(aOrigin, origin);

  if (headerValue == origin) {
    *_retval = true;
    return NS_OK;
  }

  *_retval = false;
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
HttpBaseChannel::GetInitiatorType(nsAString & aInitiatorType)
{
  aInitiatorType = mInitiatorType;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetInitiatorType(const nsAString & aInitiatorType)
{
  mInitiatorType = aInitiatorType;
  return NS_OK;
}

#define IMPL_TIMING_ATTR(name)                                 \
NS_IMETHODIMP                                                  \
HttpBaseChannel::Get##name##Time(PRTime* _retval) {            \
    TimeStamp stamp;                                           \
    Get##name(&stamp);                                         \
    if (stamp.IsNull()) {                                      \
        *_retval = 0;                                          \
        return NS_OK;                                          \
    }                                                          \
    *_retval = mChannelCreationTime +                          \
        (PRTime) ((stamp - mChannelCreationTimestamp).ToSeconds() * 1e6); \
    return NS_OK;                                              \
}

IMPL_TIMING_ATTR(ChannelCreation)
IMPL_TIMING_ATTR(AsyncOpen)
IMPL_TIMING_ATTR(DomainLookupStart)
IMPL_TIMING_ATTR(DomainLookupEnd)
IMPL_TIMING_ATTR(ConnectStart)
IMPL_TIMING_ATTR(ConnectEnd)
IMPL_TIMING_ATTR(RequestStart)
IMPL_TIMING_ATTR(ResponseStart)
IMPL_TIMING_ATTR(ResponseEnd)
IMPL_TIMING_ATTR(CacheReadStart)
IMPL_TIMING_ATTR(CacheReadEnd)
IMPL_TIMING_ATTR(RedirectStart)
IMPL_TIMING_ATTR(RedirectEnd)

#undef IMPL_TIMING_ATTR

nsPerformance*
HttpBaseChannel::GetPerformance()
{
    // If performance timing is disabled, there is no need for the nsPerformance
    // object anymore.
    if (!mTimingEnabled) {
        return nullptr;
    }
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(this, loadContext);
    if (!loadContext) {
        return nullptr;
    }
    nsCOMPtr<nsIDOMWindow> domWindow;
    loadContext->GetAssociatedWindow(getter_AddRefs(domWindow));
    if (!domWindow) {
        return nullptr;
    }
    nsCOMPtr<nsPIDOMWindow> pDomWindow = do_QueryInterface(domWindow);
    if (!pDomWindow) {
        return nullptr;
    }
    if (!pDomWindow->IsInnerWindow()) {
        pDomWindow = pDomWindow->GetCurrentInnerWindow();
        if (!pDomWindow) {
            return nullptr;
        }
    }

    nsPerformance* docPerformance = pDomWindow->GetPerformance();
    if (!docPerformance) {
      return nullptr;
    }
    // iframes should be added to the parent's entries list.
    if (mLoadFlags & LOAD_DOCUMENT_URI) {
      return docPerformance->GetParentPerformance();
    }
    return docPerformance;
}

//------------------------------------------------------------------------------

bool
HttpBaseChannel::EnsureSchedulingContextID()
{
    nsID nullID;
    nullID.Clear();
    if (!mSchedulingContextID.Equals(nullID)) {
        // Already have a scheduling context ID, no need to do the rest of this work
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

    // Set the load group connection scope on the transaction
    rootLoadGroup->GetSchedulingContextID(&mSchedulingContextID);
    return true;
}

NS_IMETHODIMP
HttpBaseChannel::SetCorsPreflightParameters(const nsTArray<nsCString>& aUnsafeHeaders,
                                            bool aWithCredentials,
                                            nsIPrincipal* aPrincipal)
{
  ENSURE_CALLED_BEFORE_CONNECT();

  mRequireCORSPreflight = true;
  mUnsafeHeaders = aUnsafeHeaders;
  mWithCredentials = aWithCredentials;
  mPreflightPrincipal = aPrincipal;
  return NS_OK;
}

} // namespace net
} // namespace mozilla

