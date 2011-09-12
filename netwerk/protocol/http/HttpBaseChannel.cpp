/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* ***** BEGIN LICENSE BLOCK *****
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
 *  The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Daniel Witte <dwitte@mozilla.com>
 *   Frederic Plourde <bugzillaFred@gmail.com>
 *   Jason Duell <jduell.mcbugs@gmail.com>
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

#include "mozilla/net/HttpBaseChannel.h"

#include "nsHttpHandler.h"
#include "nsMimeTypes.h"
#include "nsNetUtil.h"

#include "nsICachingChannel.h"
#include "nsISeekableStream.h"
#include "nsITimedChannel.h"
#include "nsIEncodedChannel.h"
#include "nsIResumableChannel.h"
#include "nsIApplicationCacheChannel.h"
#include "nsEscape.h"

#include "prnetdb.h"

namespace mozilla {
namespace net {

HttpBaseChannel::HttpBaseChannel()
  : mStartPos(LL_MAXUINT)
  , mStatus(NS_OK)
  , mLoadFlags(LOAD_NORMAL)
  , mPriority(PRIORITY_NORMAL)
  , mCaps(0)
  , mRedirectionLimit(gHttpHandler->RedirectionLimit())
  , mApplyConversion(PR_TRUE)
  , mCanceled(PR_FALSE)
  , mIsPending(PR_FALSE)
  , mWasOpened(PR_FALSE)
  , mResponseHeadersModified(PR_FALSE)
  , mAllowPipelining(PR_TRUE)
  , mForceAllowThirdPartyCookie(PR_FALSE)
  , mUploadStreamHasHeaders(PR_FALSE)
  , mInheritApplicationCache(PR_TRUE)
  , mChooseApplicationCache(PR_FALSE)
  , mLoadedFromApplicationCache(PR_FALSE)
  , mChannelIsForDownload(PR_FALSE)
  , mTracingEnabled(PR_TRUE)
  , mTimingEnabled(PR_FALSE)
  , mSuspendCount(0)
  , mRedirectedCachekeys(nsnull)
{
  LOG(("Creating HttpBaseChannel @%x\n", this));

  // grab a reference to the handler to ensure that it doesn't go away.
  NS_ADDREF(gHttpHandler);

  // Subfields of unions cannot be targeted in an initializer list
  mSelfAddr.raw.family = PR_AF_UNSPEC;
  mPeerAddr.raw.family = PR_AF_UNSPEC;
}

HttpBaseChannel::~HttpBaseChannel()
{
  LOG(("Destroying HttpBaseChannel @%x\n", this));

  // Make sure we don't leak
  CleanRedirectCacheChainIfNecessary();

  gHttpHandler->Release();
}

nsresult
HttpBaseChannel::Init(nsIURI *aURI,
                      PRUint8 aCaps,
                      nsProxyInfo *aProxyInfo)
{
  LOG(("HttpBaseChannel::Init [this=%p]\n", this));

  NS_PRECONDITION(aURI, "null uri");

  nsresult rv = nsHashPropertyBag::Init();
  if (NS_FAILED(rv)) return rv;

  mURI = aURI;
  mOriginalURI = aURI;
  mDocumentURI = nsnull;
  mCaps = aCaps;

  // Construct connection info object
  nsCAutoString host;
  PRInt32 port = -1;
  PRBool usingSSL = PR_FALSE;

  rv = mURI->SchemeIs("https", &usingSSL);
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

  mConnectionInfo = new nsHttpConnectionInfo(host, port,
                                             aProxyInfo, usingSSL);
  if (!mConnectionInfo)
    return NS_ERROR_OUT_OF_MEMORY;

  // Set default request method
  mRequestHead.SetMethod(nsHttp::Get);

  // Set request headers
  nsCAutoString hostLine;
  rv = nsHttpHandler::GenerateHostPort(host, port, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = mRequestHead.SetHeader(nsHttp::Host, hostLine);
  if (NS_FAILED(rv)) return rv;

  rv = gHttpHandler->
      AddStandardRequestHeaders(&mRequestHead.Headers(), aCaps,
                                !mConnectionInfo->UsingSSL() &&
                                mConnectionInfo->UsingHttpProxy());

  return rv;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS_INHERITED9(HttpBaseChannel,
                             nsHashPropertyBag, 
                             nsIRequest,
                             nsIChannel,
                             nsIEncodedChannel,
                             nsIHttpChannel,
                             nsIHttpChannelInternal,
                             nsIUploadChannel,
                             nsIUploadChannel2,
                             nsISupportsPriority,
                             nsITraceableChannel)

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
HttpBaseChannel::IsPending(PRBool *aIsPending)
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
  mLoadGroup = aLoadGroup;
  mProgressSink = nsnull;
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
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

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
  mCallbacks = aCallbacks;
  mProgressSink = nsnull;
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

    nsCAutoString contentTypeBuf, charsetBuf;
    PRBool hadCharset;
    net_ParseContentType(aContentType, contentTypeBuf, charsetBuf, &hadCharset);

    mResponseHead->SetContentType(contentTypeBuf);

    // take care not to stomp on an existing charset
    if (hadCharset)
      mResponseHead->SetContentCharset(charsetBuf);

  } else {
    // We are being given a content-type hint.
    PRBool dummy;
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
HttpBaseChannel::GetContentDisposition(PRUint32 *aContentDisposition)
{
  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_FAILED(rv))
    return rv;

  *aContentDisposition = NS_GetContentDispositionFromHeader(header, this);

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentDispositionFilename(nsAString& aContentDispositionFilename)
{
  aContentDispositionFilename.Truncate();

  nsresult rv;
  nsCString header;

  rv = GetContentDispositionHeader(header);
  if (NS_FAILED(rv))
    return rv;

  return NS_GetFilenameFromDisposition(aContentDispositionFilename,
                                       header, mURI);
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
HttpBaseChannel::GetContentLength(PRInt32 *aContentLength)
{
  NS_ENSURE_ARG_POINTER(aContentLength);

  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;

  // XXX truncates to 32 bit
  *aContentLength = mResponseHead->ContentLength();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetContentLength(PRInt32 value)
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
                               PRInt32 contentLength)
{
  // NOTE: for backwards compatibility and for compatibility with old style
  // plugins, |stream| may include headers, specifically Content-Type and
  // Content-Length headers.  in this case, |contentType| and |contentLength|
  // would be unspecified.  this is traditionally the case of a POST request,
  // and so we select POST as the request method if contentType and
  // contentLength are unspecified.

  if (stream) {
    if (contentType.IsEmpty()) {
      mUploadStreamHasHeaders = PR_TRUE;
      mRequestHead.SetMethod(nsHttp::Post); // POST request
    } else {
      if (contentLength < 0) {
        // Not really kosher to assume Available == total length of
        // stream, but apparently works for the streams we see here.
        stream->Available((PRUint32 *) &contentLength);
        if (contentLength < 0) {
          NS_ERROR("unable to determine content length");
          return NS_ERROR_FAILURE;
        }
      }
      // SetRequestHeader propagates headers to chrome if HttpChannelChild 
      nsCAutoString contentLengthStr;
      contentLengthStr.AppendInt(PRInt64(contentLength));
      SetRequestHeader(NS_LITERAL_CSTRING("Content-Length"), contentLengthStr, 
                       PR_FALSE);
      SetRequestHeader(NS_LITERAL_CSTRING("Content-Type"), contentType, 
                       PR_FALSE);
      mUploadStreamHasHeaders = PR_FALSE;
      mRequestHead.SetMethod(nsHttp::Put); // PUT request
    }
  } else {
    mUploadStreamHasHeaders = PR_FALSE;
    mRequestHead.SetMethod(nsHttp::Get); // revert to GET request
  }
  mUploadStream = stream;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIUploadChannel2
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::ExplicitSetUploadStream(nsIInputStream *aStream,
                                       const nsACString &aContentType,
                                       PRInt64 aContentLength,
                                       const nsACString &aMethod,
                                       PRBool aStreamHasHeaders)
{
  // Ensure stream is set and method is valid 
  NS_ENSURE_TRUE(aStream, NS_ERROR_FAILURE);

  if (aContentLength < 0 && !aStreamHasHeaders) {
    PRUint32 streamLength;
    aStream->Available(&streamLength);
    aContentLength = streamLength;
    if (aContentLength < 0) {
      NS_ERROR("unable to determine content length");
      return NS_ERROR_FAILURE;
    }
  }

  nsresult rv = SetRequestMethod(aMethod);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aStreamHasHeaders) {
    // SetRequestHeader propagates headers to chrome if HttpChannelChild 
    nsCAutoString contentLengthStr;
    contentLengthStr.AppendInt(aContentLength);
    SetRequestHeader(NS_LITERAL_CSTRING("Content-Length"), contentLengthStr, 
                     PR_FALSE);
    SetRequestHeader(NS_LITERAL_CSTRING("Content-Type"), aContentType, 
                     PR_FALSE);
  }

  mUploadStreamHasHeaders = aStreamHasHeaders;
  mUploadStream = aStream;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetUploadStreamHasHeaders(PRBool *hasHeaders)
{
  NS_ENSURE_ARG(hasHeaders);

  *hasHeaders = mUploadStreamHasHeaders;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsIEncodedChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetApplyConversion(PRBool *value)
{
  *value = mApplyConversion;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetApplyConversion(PRBool value)
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

  const char *val = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
  if (gHttpHandler->IsAcceptableEncoding(val)) {
    nsCOMPtr<nsIStreamConverterService> serv;
    nsresult rv = gHttpHandler->
            GetStreamConverterService(getter_AddRefs(serv));
    // we won't fail to load the page just because we couldn't load the
    // stream converter service.. carry on..
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStreamListener> converter;
      nsCAutoString from(val);
      ToLowerCase(from);
      rv = serv->AsyncConvertData(from.get(),
                                  "uncompressed",
                                  mListener,
                                  mListenerContext,
                                  getter_AddRefs(converter));
      if (NS_SUCCEEDED(rv)) {
        LOG(("converter installed from \'%s\' to \'uncompressed\'\n", val));
        mListener = converter;
      }
    }
  } else if (val != nsnull) {
    LOG(("Unknown content encoding '%s', ignoring\n", val));
  }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetContentEncodings(nsIUTF8StringEnumerator** aEncodings)
{
  if (!mResponseHead) {
    *aEncodings = nsnull;
    return NS_OK;
  }
    
  const char *encoding = mResponseHead->PeekHeader(nsHttp::Content_Encoding);
  if (!encoding) {
    *aEncodings = nsnull;
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
  , mReady(PR_FALSE)
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
HttpBaseChannel::nsContentEncodings::HasMore(PRBool* aMoreEncodings)
{
  if (mReady) {
    *aMoreEncodings = PR_TRUE;
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

  PRBool haveType = PR_FALSE;
  if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("gzip"), start, end)) {
    aNextEncoding.AssignLiteral(APPLICATION_GZIP);
    haveType = PR_TRUE;
  }

  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("compress"), start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_COMPRESS);
      haveType = PR_TRUE;
    }
  }
    
  if (!haveType) {
    encoding.BeginReading(start);
    if (CaseInsensitiveFindInReadable(NS_LITERAL_CSTRING("deflate"), start, end)) {
      aNextEncoding.AssignLiteral(APPLICATION_ZIP);
      haveType = PR_TRUE;
    }
  }

  // Prepare to fetch the next encoding
  mCurEnd = mCurStart;
  mReady = PR_FALSE;
  
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
  NS_ASSERTION(mCurStart == mCurEnd, "Indeterminate state");
    
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
        
  mReady = PR_TRUE;
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
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  const nsCString& flatMethod = PromiseFlatCString(aMethod);

  // Method names are restricted to valid HTTP tokens.
  if (!nsHttp::IsValidToken(flatMethod))
    return NS_ERROR_INVALID_ARG;

  nsHttpAtom atom = nsHttp::ResolveAtom(flatMethod.get());
  if (!atom)
    return NS_ERROR_FAILURE;

  mRequestHead.SetMethod(atom);
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
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  // clear existing referrer, if any
  mReferrer = nsnull;
  mRequestHead.ClearHeader(nsHttp::Referer);

  if (!referrer)
      return NS_OK;

  // check referrer blocking pref
  PRUint32 referrerLevel;
  if (mLoadFlags & LOAD_INITIAL_DOCUMENT_URI)
    referrerLevel = 1; // user action
  else
    referrerLevel = 2; // inline content
  if (gHttpHandler->ReferrerLevel() < referrerLevel)
    return NS_OK;

  nsCOMPtr<nsIURI> referrerGrip;
  nsresult rv;
  PRBool match;

  //
  // Strip off "wyciwyg://123/" from wyciwyg referrers.
  //
  // XXX this really belongs elsewhere since wyciwyg URLs aren't part of necko.
  //     perhaps some sort of generic nsINestedURI could be used.  then, if an URI
  //     fails the whitelist test, then we could check for an inner URI and try
  //     that instead.  though, that might be too automatic.
  // 
  rv = referrer->SchemeIs("wyciwyg", &match);
  if (NS_FAILED(rv)) return rv;
  if (match) {
    nsCAutoString path;
    rv = referrer->GetPath(path);
    if (NS_FAILED(rv)) return rv;

    PRUint32 pathLength = path.Length();
    if (pathLength <= 2) return NS_ERROR_FAILURE;

    // Path is of the form "//123/http://foo/bar", with a variable number of digits.
    // To figure out where the "real" URL starts, search path for a '/', starting at 
    // the third character.
    PRInt32 slashIndex = path.FindChar('/', 2);
    if (slashIndex == kNotFound) return NS_ERROR_FAILURE;

    // Get the charset of the original URI so we can pass it to our fixed up URI.
    nsCAutoString charset;
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
    "gopher",
    nsnull
  };
  match = PR_FALSE;
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
      nsCAutoString referrerHost;
      nsCAutoString host;

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

  // strip away any userpass; we don't want to be giving out passwords ;-)
  rv = clone->SetUserPass(EmptyCString());
  if (NS_FAILED(rv)) return rv;

  nsCAutoString spec;
  rv = clone->GetAsciiSpec(spec);
  if (NS_FAILED(rv)) return rv;

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
                                  PRBool aMerge)
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
                                   PRBool merge)
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

  mResponseHeadersModified = PR_TRUE;

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
HttpBaseChannel::GetAllowPipelining(PRBool *value)
{
  NS_ENSURE_ARG_POINTER(value);
  *value = mAllowPipelining;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetAllowPipelining(PRBool value)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mAllowPipelining = value;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRedirectionLimit(PRUint32 *value)
{
  NS_ENSURE_ARG_POINTER(value);
  *value = mRedirectionLimit;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetRedirectionLimit(PRUint32 value)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mRedirectionLimit = NS_MIN<PRUint32>(value, 0xff);
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoStoreResponse(PRBool *value)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoStore();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::IsNoCacheResponse(PRBool *value)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  *value = mResponseHead->NoCache();
  if (!*value)
    *value = mResponseHead->ExpiresInPast();
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseStatus(PRUint32 *aValue)
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
HttpBaseChannel::GetRequestSucceeded(PRBool *aValue)
{
  if (!mResponseHead)
    return NS_ERROR_NOT_AVAILABLE;
  PRUint32 status = mResponseHead->Status();
  *aValue = (status / 100 == 2);
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
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mDocumentURI = aDocumentURI;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRequestVersion(PRUint32 *major, PRUint32 *minor)
{
  nsHttpVersion version = mRequestHead.Version();

  if (major) { *major = version / 10; }
  if (minor) { *minor = version % 10; }

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetResponseVersion(PRUint32 *major, PRUint32 *minor)
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

  return cs->SetCookieStringFromHttp(mURI, nsnull, nsnull, aCookieHeader,
                                     mResponseHead->PeekHeader(nsHttp::Date),
                                     this);
}

NS_IMETHODIMP
HttpBaseChannel::GetForceAllowThirdPartyCookie(PRBool *aForce)
{
  *aForce = mForceAllowThirdPartyCookie;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetForceAllowThirdPartyCookie(PRBool aForce)
{
  ENSURE_CALLED_BEFORE_ASYNC_OPEN();

  mForceAllowThirdPartyCookie = aForce;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetCanceled(PRBool *aCanceled)
{
  *aCanceled = mCanceled;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetChannelIsForDownload(PRBool *aChannelIsForDownload)
{
  *aChannelIsForDownload = mChannelIsForDownload;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::SetChannelIsForDownload(PRBool aChannelIsForDownload)
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

  addr.SetCapacity(64);
  PR_NetAddrToString(&mSelfAddr, addr.BeginWriting(), 64);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetLocalPort(PRInt32* port)
{
  NS_ENSURE_ARG_POINTER(port);

  if (mSelfAddr.raw.family == PR_AF_INET) {
    *port = (PRInt32)PR_ntohs(mSelfAddr.inet.port);
  }
  else if (mSelfAddr.raw.family == PR_AF_INET6) {
    *port = (PRInt32)PR_ntohs(mSelfAddr.ipv6.port);
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

  addr.SetCapacity(64);
  PR_NetAddrToString(&mPeerAddr, addr.BeginWriting(), 64);
  addr.SetLength(strlen(addr.BeginReading()));

  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::GetRemotePort(PRInt32* port)
{
  NS_ENSURE_ARG_POINTER(port);

  if (mPeerAddr.raw.family == PR_AF_INET) {
    *port = (PRInt32)PR_ntohs(mPeerAddr.inet.port);
  }
  else if (mPeerAddr.raw.family == PR_AF_INET6) {
    *port = (PRInt32)PR_ntohs(mPeerAddr.ipv6.port);
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

//-----------------------------------------------------------------------------
// HttpBaseChannel::nsISupportsPriority
//-----------------------------------------------------------------------------

NS_IMETHODIMP
HttpBaseChannel::GetPriority(PRInt32 *value)
{
  *value = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
HttpBaseChannel::AdjustPriority(PRInt32 delta)
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
  if (mRequestHead.Method() != nsHttp::Get) {
    return NS_ERROR_NOT_RESUMABLE;
  }

  PRUint64 size = LL_MAXUINT;
  nsCAutoString etag, lastmod;
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
  entityID.AppendInt(PRInt64(size));
  entityID.Append('/');
  entityID.Append(lastmod);
  // NOTE: Appending lastmod as the last part avoids having to escape it

  aEntityID = entityID;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsStreamListenerWrapper <private>
//-----------------------------------------------------------------------------

// Wrapper class to make replacement of nsHttpChannel's listener
// from JavaScript possible. It is workaround for bug 433711.
class nsStreamListenerWrapper : public nsIStreamListener
{
public:
  nsStreamListenerWrapper(nsIStreamListener *listener);

  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIREQUESTOBSERVER(mListener->)
  NS_FORWARD_NSISTREAMLISTENER(mListener->)

private:
  ~nsStreamListenerWrapper() {}
  nsCOMPtr<nsIStreamListener> mListener;
};

nsStreamListenerWrapper::nsStreamListenerWrapper(nsIStreamListener *listener)
  : mListener(listener)
{
  NS_ASSERTION(mListener, "no stream listener specified");
}

NS_IMPL_ISUPPORTS2(nsStreamListenerWrapper,
                   nsIStreamListener,
                   nsIRequestObserver)

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
HttpBaseChannel::DoNotifyListener()
{
  // Make sure mIsPending is set to PR_FALSE. At this moment we are done from
  // the point of view of our consumer and we have to report our self
  // as not-pending.
  if (mListener) {
    mListener->OnStartRequest(this, mListenerContext);
    mIsPending = PR_FALSE;
    mListener->OnStopRequest(this, mListenerContext, mStatus);
    mListener = 0;
    mListenerContext = 0;
  } else {
    mIsPending = PR_FALSE;
  }
  // We have to make sure to drop the reference to the callbacks too
  mCallbacks = nsnull;
  mProgressSink = nsnull;

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
                                  nsnull,
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
  SetRequestHeader(nsDependentCString(nsHttp::Cookie), cookie, PR_FALSE);
}

static PLDHashOperator
CopyProperties(const nsAString& aKey, nsIVariant *aData, void *aClosure)
{
  nsIWritablePropertyBag* bag = static_cast<nsIWritablePropertyBag*>
                                           (aClosure);
  bag->SetProperty(aKey, aData);
  return PL_DHASH_NEXT;
}

nsresult
HttpBaseChannel::SetupReplacementChannel(nsIURI       *newURI, 
                                         nsIChannel   *newChannel,
                                         PRBool        preserveMethod)
{
  LOG(("HttpBaseChannel::SetupReplacementChannel "
     "[this=%p newChannel=%p preserveMethod=%d]",
     this, newChannel, preserveMethod));
  PRUint32 newLoadFlags = mLoadFlags | LOAD_REPLACE;
  // if the original channel was using SSL and this channel is not using
  // SSL, then no need to inhibit persistent caching.  however, if the
  // original channel was not using SSL and has INHIBIT_PERSISTENT_CACHING
  // set, then allow the flag to apply to the redirected channel as well.
  // since we force set INHIBIT_PERSISTENT_CACHING on all HTTPS channels,
  // we only need to check if the original channel was using SSL.
  if (mConnectionInfo->UsingSSL())
    newLoadFlags &= ~INHIBIT_PERSISTENT_CACHING;

  // Do not pass along LOAD_CHECK_OFFLINE_CACHE
  newLoadFlags &= ~nsICachingChannel::LOAD_CHECK_OFFLINE_CACHE;

  newChannel->SetLoadGroup(mLoadGroup); 
  newChannel->SetNotificationCallbacks(mCallbacks);
  newChannel->SetLoadFlags(newLoadFlags);

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
        PRInt64 len = clen ? nsCRT::atoll(clen) : -1;
        uploadChannel2->ExplicitSetUploadStream(
                                  mUploadStream, nsDependentCString(ctype), len,
                                  nsDependentCString(mRequestHead.Method()),
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
                                           atoi(clen));
          }
        }
      }
    }
    // since preserveMethod is true, we need to ensure that the appropriate 
    // request method gets set on the channel, regardless of whether or not 
    // we set the upload stream above. This means SetRequestMethod() will
    // be called twice if ExplicitSetUploadStream() gets called above.

    httpChannel->SetRequestMethod(nsDependentCString(mRequestHead.Method()));
  }
  // convey the referrer if one was used for this channel to the next one
  if (mReferrer)
    httpChannel->SetReferrer(mReferrer);
  // convey the mAllowPipelining flag
  httpChannel->SetAllowPipelining(mAllowPipelining);
  // convey the new redirection limit
  httpChannel->SetRedirectionLimit(mRedirectionLimit - 1);

  nsCOMPtr<nsIHttpChannelInternal> httpInternal = do_QueryInterface(newChannel);
  if (httpInternal) {
    // convey the mForceAllowThirdPartyCookie flag
    httpInternal->SetForceAllowThirdPartyCookie(mForceAllowThirdPartyCookie);

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
        httpInternal->SetCacheKeysRedirectChain(mRedirectedCachekeys);
        mRedirectedCachekeys = nsnull;
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

