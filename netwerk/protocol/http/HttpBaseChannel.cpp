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
#include "nsNetUtil.h"

#include "nsICachingChannel.h"
#include "nsISeekableStream.h"
#include "nsITimedChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIApplicationCacheChannel.h"
#include "nsEscape.h"
#include "nsStreamListenerWrapper.h"
#include "nsISecurityConsoleMessage.h"
#include "nsURLHelper.h"
#include "nsICookieService.h"
#include "nsIStreamConverterService.h"
#include "nsCRT.h"
#include "nsIObserverService.h"

#include <algorithm>

namespace mozilla {
namespace net {

HttpBaseChannel::HttpBaseChannel()
  : mStartPos(UINT64_MAX)
  , mStatus(NS_OK)
  , mLoadFlags(LOAD_NORMAL)
  , mCaps(0)
  , mPriority(PRIORITY_NORMAL)
  , mRedirectionLimit(gHttpHandler->RedirectionLimit())
  , mApplyConversion(true)
  , mCanceled(false)
  , mIsPending(false)
  , mWasOpened(false)
  , mRequestObserversCalled(false)
  , mResponseHeadersModified(false)
  , mAllowPipelining(true)
  , mForceAllowThirdPartyCookie(false)
  , mUploadStreamHasHeaders(false)
  , mInheritApplicationCache(true)
  , mChooseApplicationCache(false)
  , mLoadedFromApplicationCache(false)
  , mChannelIsForDownload(false)
  , mTracingEnabled(true)
  , mTimingEnabled(false)
  , mAllowSpdy(true)
  , mLoadAsBlocking(false)
  , mLoadUnblocked(false)
  , mResponseTimeoutEnabled(true)
  , mSuspendCount(0)
  , mProxyResolveFlags(0)
  , mContentDispositionHint(UINT32_MAX)
  , mHttpHandler(gHttpHandler)
{
  LOG(("Creating HttpBaseChannel @%x\n", this));

  // Subfields of unions cannot be targeted in an initializer list
  mSelfAddr.raw.family = PR_AF_UNSPEC;
  mPeerAddr.raw.family = PR_AF_UNSPEC;
}

HttpBaseChannel::~HttpBaseChannel()
{
  LOG(("Destroying HttpBaseChannel @%x\n", this));

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
  bool usingSSL = false;

  nsresult rv = mURI->SchemeIs("https", &usingSSL);
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

  rv = gHttpHandler->AddStandardRequestHeaders(&mRequestHead.Headers());
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

NS_IMPL_ISUPPORTS_INHERITED10(HttpBaseChannel,
                              nsHashPropertyBag,
                              nsIRequest,
                              nsIChannel,
                              nsIEncodedChannel,
                              nsIHttpChannel,
                              nsIHttpChannelInternal,
                              nsIUploadChannel,
                              nsIUploadChannel2,
                              nsISupportsPriority,
                              nsITraceableChannel,
                              nsIPrivateBrowsingChannel)

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
  *aIsPending = mIsPending;
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
HttpBaseChannel::ApplyContentConversions()
{
  if (!mResponseHead)
    return NS_OK;

  LOG(("HttpBaseChannel::ApplyContentConversions [this=%p]\n", this));

  if (!mApplyConversion) {
    LOG(("not applying conversion per mApplyConversion\n"));
    return NS_OK;
  }

  nsAutoCString contentEncoding;
  char *cePtr, *val;
  nsresult rv;

  rv = mResponseHead->GetHeader(nsHttp::Content_Encoding, contentEncoding);
  if (NS_FAILED(rv) || contentEncoding.IsEmpty())
    return NS_OK;

  // The encodings are listed in the order they were applied
  // (see rfc 2616 section 14.11), so they need to removed in reverse
  // order. This is accomplished because the converter chain ends up
  // being a stack with the last converter created being the first one
  // to accept the raw network data.

  cePtr = contentEncoding.BeginWriting();
  uint32_t count = 0;
  while ((val = nsCRT::strtok(cePtr, HTTP_LWS ",", &cePtr))) {
    if (++count > 16) {
      // That's ridiculous. We only understand 2 different ones :)
      // but for compatibility with old code, we will just carry on without
      // removing the encodings
      LOG(("Too many Content-Encodings. Ignoring remainder.\n"));
      break;
    }

    if (gHttpHandler->IsAcceptableEncoding(val)) {
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
                                  mListener,
                                  mListenerContext,
                                  getter_AddRefs(converter));
      if (NS_FAILED(rv)) {
        LOG(("Unexpected failure of AsyncConvertData %s\n", val));
        return rv;
      }

      LOG(("converter removed '%s' content-encoding\n", val));
      mListener = converter;
    }
    else {
      if (val)
        LOG(("Unknown content encoding '%s', ignoring\n", val));
    }
  }

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

NS_IMPL_ISUPPORTS1(HttpBaseChannel::nsContentEncodings, nsIUTF8StringEnumerator)

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
  ENSURE_CALLED_BEFORE_CONNECT();

  // clear existing referrer, if any
  mReferrer = nullptr;
  mRequestHead.ClearHeader(nsHttp::Referer);

  if (!referrer)
      return NS_OK;

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
  if (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI)
    referrerLevel = 1; // user action
  else
    referrerLevel = 2; // inline content
  if (userReferrerLevel < referrerLevel)
    return NS_OK;

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
  if (!match)
    return NS_OK; // kick out....

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
    if (!match)
      return NS_OK;

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

  nsCOMPtr<nsIURI> clone;
  //
  // we need to clone the referrer, so we can:
  //  (1) modify it
  //  (2) keep a reference to it after returning from this function
  //
  // Use CloneIgnoringRef to strip away any fragment per RFC 2616 section 14.36
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
  rv = clone->SetUserPass(EmptyCString());
  if (NS_FAILED(rv)) return rv;

  nsAutoCString spec;

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
  mReferrer = clone;
  mRequestHead.SetHeader(nsHttp::Referer, spec);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestHeader(const nsACString& aHeader,
                                  nsACString& aValue)
{
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

  // Header names are restricted to valid HTTP tokens.
  if (!nsHttp::IsValidToken(flatHeader))
    return NS_ERROR_INVALID_ARG;

  // Header values MUST NOT contain line-breaks.  RFC 2616 technically
  // permits CTL characters, including CR and LF, in header values provided
  // they are quoted.  However, this can lead to problems if servers do not
  // interpret quoted strings properly.  Disallowing CR and LF here seems
  // reasonable and keeps things simple.  We also disallow a null byte.
  if (flatValue.FindCharInSet("\r\n") != kNotFound ||
      flatValue.Length() != strlen(flatValue.get()))
    return NS_ERROR_INVALID_ARG;

  nsHttpAtom atom = nsHttp::ResolveAtom(flatHeader.get());
  if (!atom) {
    NS_WARNING("failed to resolve atom");
    return NS_ERROR_NOT_AVAILABLE;
  }

  return mRequestHead.SetHeader(atom, flatValue, aMerge);
}

NS_IMETHODIMP
HttpBaseChannel::VisitRequestHeaders(nsIHttpHeaderVisitor *visitor)
{
  return mRequestHead.Headers().VisitHeaders(visitor);
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseHeader(const nsACString &header, nsACString &value)
{
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

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIHttpChannelInternal
//-----------------------------------------------------------------------------

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

} // anonymous namespace

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
HttpBaseChannel::GetForceAllowThirdPartyCookie(bool *aForce)
{
  *aForce = mForceAllowThirdPartyCookie;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetForceAllowThirdPartyCookie(bool aForce)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mForceAllowThirdPartyCookie = aForce;
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
NS_IMETHODIMP
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
HttpBaseChannel::GetLoadAsBlocking(bool *aLoadAsBlocking)
{
  NS_ENSURE_ARG_POINTER(aLoadAsBlocking);
  *aLoadAsBlocking = mLoadAsBlocking;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadAsBlocking(bool aLoadAsBlocking)
{
  mLoadAsBlocking = aLoadAsBlocking;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLoadUnblocked(bool *aLoadUnblocked)
{
  NS_ENSURE_ARG_POINTER(aLoadUnblocked);
  *aLoadUnblocked = mLoadUnblocked;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetLoadUnblocked(bool aLoadUnblocked)
{
  mLoadUnblocked = aLoadUnblocked;
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
  // Make sure mIsPending is set to false. At this moment we are done from
  // the point of view of our consumer and we have to report our self
  // as not-pending.
  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mIsPending = false;
    mListener->OnStopRequest(this, mListenerContext, mStatus);
  } else {
    mIsPending = false;
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
    (XRE_GetProcessType() == GeckoProcessType_Default);
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
      cookie.Append(NS_LITERAL_CSTRING("; ") + mUserSetCookieHeader);
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

  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(newChannel);
  if (!httpChannel)
    return NS_OK; // no other options to set

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
    httpChannel->SetReferrer(mReferrer);
  // convey the mAllowPipelining flag
  httpChannel->SetAllowPipelining(mAllowPipelining);
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

  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(newChannel);
  if (httpInternal) {
    // convey the mForceAllowThirdPartyCookie flag
    httpInternal->SetForceAllowThirdPartyCookie(mForceAllowThirdPartyCookie);
    // convey the spdy flag
    httpInternal->SetAllowSpdy(mAllowSpdy);

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

  // transfer timed channel enabled status
  nsCOMPtr<nsITimedChannel> timed(do_QueryInterface(newChannel));
  if (timed)
    timed->SetTimingEnabled(mTimingEnabled);

  return NS_OK;
}

//------------------------------------------------------------------------------

}  // namespace net
}  // namespace mozilla

