/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsMultiMixedConv.h"
#include "nsIHttpChannel.h"
#include "nsIThreadRetargetableStreamListener.h"
#include "nsNetCID.h"
#include "nsMimeTypes.h"
#include "nsIStringStream.h"
#include "nsCRT.h"
#include "nsIHttpChannelInternal.h"
#include "nsURLHelper.h"
#include "nsIStreamConverterService.h"
#include <algorithm>
#include "nsContentSecurityManager.h"
#include "nsHttp.h"
#include "nsNetUtil.h"
#include "nsIURI.h"
#include "nsHttpHeaderArray.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Components.h"
#include "mozilla/Tokenizer.h"
#include "nsComponentManagerUtils.h"
#include "mozilla/StaticPrefs_network.h"

using namespace mozilla;

nsPartChannel::nsPartChannel(nsIChannel* aMultipartChannel, uint32_t aPartID,
                             bool aIsFirstPart, nsIStreamListener* aListener)
    : mMultipartChannel(aMultipartChannel),
      mListener(aListener),
      mPartID(aPartID),
      mIsFirstPart(aIsFirstPart) {
  // Inherit the load flags from the original channel...
  mMultipartChannel->GetLoadFlags(&mLoadFlags);

  mMultipartChannel->GetLoadGroup(getter_AddRefs(mLoadGroup));
}

void nsPartChannel::InitializeByteRange(int64_t aStart, int64_t aEnd) {
  mIsByteRangeRequest = true;

  mByteRangeStart = aStart;
  mByteRangeEnd = aEnd;
}

nsresult nsPartChannel::SendOnStartRequest(nsISupports* aContext) {
  return mListener->OnStartRequest(this);
}

nsresult nsPartChannel::SendOnDataAvailable(nsISupports* aContext,
                                            nsIInputStream* aStream,
                                            uint64_t aOffset, uint32_t aLen) {
  return mListener->OnDataAvailable(this, aStream, aOffset, aLen);
}

nsresult nsPartChannel::SendOnStopRequest(nsISupports* aContext,
                                          nsresult aStatus) {
  // Drop the listener
  nsCOMPtr<nsIStreamListener> listener;
  listener.swap(mListener);
  return listener->OnStopRequest(this, aStatus);
}

void nsPartChannel::SetContentDisposition(
    const nsACString& aContentDispositionHeader) {
  mContentDispositionHeader = aContentDispositionHeader;
  nsCOMPtr<nsIURI> uri;
  GetURI(getter_AddRefs(uri));
  NS_GetFilenameFromDisposition(mContentDispositionFilename,
                                mContentDispositionHeader);
  mContentDisposition =
      NS_GetContentDispositionFromHeader(mContentDispositionHeader, this);
}

//
// nsISupports implementation...
//

NS_IMPL_ADDREF(nsPartChannel)
NS_IMPL_RELEASE(nsPartChannel)

NS_INTERFACE_MAP_BEGIN(nsPartChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIByteRangeRequest)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannel)
NS_INTERFACE_MAP_END

//
// nsIRequest implementation...
//

NS_IMETHODIMP
nsPartChannel::GetName(nsACString& aResult) {
  return mMultipartChannel->GetName(aResult);
}

NS_IMETHODIMP
nsPartChannel::IsPending(bool* aResult) {
  // For now, consider the active lifetime of each part the same as
  // the underlying multipart channel...  This is not exactly right,
  // but it is good enough :-)
  return mMultipartChannel->IsPending(aResult);
}

NS_IMETHODIMP
nsPartChannel::GetStatus(nsresult* aResult) {
  nsresult rv = NS_OK;

  if (NS_FAILED(mStatus)) {
    *aResult = mStatus;
  } else {
    rv = mMultipartChannel->GetStatus(aResult);
  }

  return rv;
}

NS_IMETHODIMP nsPartChannel::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsPartChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsPartChannel::CancelWithReason(nsresult aStatus,
                                              const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP
nsPartChannel::Cancel(nsresult aStatus) {
  // Cancelling an individual part must not cancel the underlying
  // multipart channel...
  // XXX but we should stop sending data for _this_ part channel!
  mStatus = aStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = NS_FAILED(mStatus);
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Suspend(void) {
  // Suspending an individual part must not suspend the underlying
  // multipart channel...
  // XXX why not?
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::Resume(void) {
  // Resuming an individual part must not resume the underlying
  // multipart channel...
  // XXX why not?
  return NS_OK;
}

//
// nsIChannel implementation
//

NS_IMETHODIMP
nsPartChannel::GetOriginalURI(nsIURI** aURI) {
  return mMultipartChannel->GetOriginalURI(aURI);
}

NS_IMETHODIMP
nsPartChannel::SetOriginalURI(nsIURI* aURI) {
  return mMultipartChannel->SetOriginalURI(aURI);
}

NS_IMETHODIMP
nsPartChannel::GetURI(nsIURI** aURI) { return mMultipartChannel->GetURI(aURI); }

NS_IMETHODIMP
nsPartChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  // This channel cannot be opened!
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPartChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  // This channel cannot be opened!
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsPartChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsPartChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsPartChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
nsPartChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = do_AddRef(mLoadGroup).take();
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;

  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetOwner(nsISupports** aOwner) {
  return mMultipartChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
nsPartChannel::SetOwner(nsISupports* aOwner) {
  return mMultipartChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
nsPartChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  return mMultipartChannel->GetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
nsPartChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  return mMultipartChannel->SetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
nsPartChannel::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks) {
  return mMultipartChannel->GetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsPartChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks) {
  return mMultipartChannel->SetNotificationCallbacks(aCallbacks);
}

NS_IMETHODIMP
nsPartChannel::GetSecurityInfo(nsITransportSecurityInfo** aSecurityInfo) {
  return mMultipartChannel->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
nsPartChannel::GetContentType(nsACString& aContentType) {
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentType(const nsACString& aContentType) {
  bool dummy;
  net_ParseContentType(aContentType, mContentType, mContentCharset, &dummy);
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentCharset(nsACString& aContentCharset) {
  aContentCharset = mContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentCharset(const nsACString& aContentCharset) {
  mContentCharset = aContentCharset;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentLength(int64_t* aContentLength) {
  *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentLength(int64_t aContentLength) {
  mContentLength = aContentLength;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  if (mContentDispositionHeader.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  *aContentDisposition = mContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPartChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  if (mContentDispositionFilename.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionFilename = mContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsPartChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  if (mContentDispositionHeader.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionHeader = mContentDispositionHeader;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetPartID(uint32_t* aPartID) {
  *aPartID = mPartID;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetIsFirstPart(bool* aIsFirstPart) {
  *aIsFirstPart = mIsFirstPart;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetIsLastPart(bool* aIsLastPart) {
  *aIsLastPart = mIsLastPart;
  return NS_OK;
}

//
// nsIByteRangeRequest implementation...
//

NS_IMETHODIMP
nsPartChannel::GetIsByteRangeRequest(bool* aIsByteRangeRequest) {
  *aIsByteRangeRequest = mIsByteRangeRequest;

  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetStartRange(int64_t* aStartRange) {
  *aStartRange = mByteRangeStart;

  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetEndRange(int64_t* aEndRange) {
  *aEndRange = mByteRangeEnd;
  return NS_OK;
}

NS_IMETHODIMP
nsPartChannel::GetBaseChannel(nsIChannel** aReturn) {
  NS_ENSURE_ARG_POINTER(aReturn);

  *aReturn = do_AddRef(mMultipartChannel).take();
  return NS_OK;
}

// nsISupports implementation
NS_IMPL_ISUPPORTS(nsMultiMixedConv, nsIStreamConverter, nsIStreamListener,
                  nsIThreadRetargetableStreamListener, nsIRequestObserver)

// nsIStreamConverter implementation

// No syncronous conversion at this time.
NS_IMETHODIMP
nsMultiMixedConv::Convert(nsIInputStream* aFromStream, const char* aFromType,
                          const char* aToType, nsISupports* aCtxt,
                          nsIInputStream** _retval) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// Stream converter service calls this to initialize the actual stream converter
// (us).
NS_IMETHODIMP
nsMultiMixedConv::AsyncConvertData(const char* aFromType, const char* aToType,
                                   nsIStreamListener* aListener,
                                   nsISupports* aCtxt) {
  NS_ASSERTION(aListener && aFromType && aToType,
               "null pointer passed into multi mixed converter");

  // hook up our final listener. this guy gets the various On*() calls we want
  // to throw at him.
  //
  // WARNING: this listener must be able to handle multiple OnStartRequest,
  // OnDataAvail() and OnStopRequest() call combinations. We call of series
  // of these for each sub-part in the raw stream.
  mFinalListener = aListener;

  return NS_OK;
}

NS_IMETHODIMP
nsMultiMixedConv::GetConvertedType(const nsACString& aFromType,
                                   nsIChannel* aChannel, nsACString& aToType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsMultiMixedConv::MaybeRetarget(nsIRequest* request) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIRequestObserver implementation
NS_IMETHODIMP
nsMultiMixedConv::OnStartRequest(nsIRequest* request) {
  // we're assuming the content-type is available at this stage
  NS_ASSERTION(mBoundary.IsEmpty(), "a second on start???");

  nsresult rv;

  mTotalSent = 0;
  mChannel = do_QueryInterface(request, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoCString contentType;

  // ask the HTTP channel for the content-type and extract the boundary from it.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel, &rv);
  if (NS_SUCCEEDED(rv)) {
    rv = httpChannel->GetResponseHeader("content-type"_ns, contentType);
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsCString csp;
    rv = httpChannel->GetResponseHeader("content-security-policy"_ns, csp);
    if (NS_SUCCEEDED(rv)) {
      mRootContentSecurityPolicy = csp;
    }
    nsCString contentDisposition;
    rv = httpChannel->GetResponseHeader("content-disposition"_ns,
                                        contentDisposition);
    if (NS_SUCCEEDED(rv)) {
      mRootContentDisposition = contentDisposition;
    }
  } else {
    // try asking the channel directly
    rv = mChannel->GetContentType(contentType);
    if (NS_FAILED(rv)) {
      return NS_ERROR_FAILURE;
    }
  }

  Tokenizer p(contentType);
  p.SkipUntil(Token::Char(';'));
  if (!p.CheckChar(';')) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }
  p.SkipWhites();
  if (!p.CheckWord("boundary")) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }
  p.SkipWhites();
  if (!p.CheckChar('=')) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }
  p.SkipWhites();
  Unused << p.ReadUntil(Token::Char(';'), mBoundary);
  mBoundary.Trim(
      " \"");  // ignoring potential quoted string formatting violations
  if (mBoundary.IsEmpty()) {
    return NS_ERROR_CORRUPTED_CONTENT;
  }

  mHeaderTokens[HEADER_CONTENT_TYPE] = mTokenizer.AddCustomToken(
      "content-type", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_CONTENT_LENGTH] = mTokenizer.AddCustomToken(
      "content-length", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_CONTENT_DISPOSITION] = mTokenizer.AddCustomToken(
      "content-disposition", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_SET_COOKIE] = mTokenizer.AddCustomToken(
      "set-cookie", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_CONTENT_RANGE] = mTokenizer.AddCustomToken(
      "content-range", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_RANGE] =
      mTokenizer.AddCustomToken("range", mTokenizer.CASE_INSENSITIVE, false);
  mHeaderTokens[HEADER_CONTENT_SECURITY_POLICY] = mTokenizer.AddCustomToken(
      "content-security-policy", mTokenizer.CASE_INSENSITIVE, false);

  mLFToken = mTokenizer.AddCustomToken("\n", mTokenizer.CASE_SENSITIVE, false);
  mCRLFToken =
      mTokenizer.AddCustomToken("\r\n", mTokenizer.CASE_SENSITIVE, false);

  SwitchToControlParsing();

  mBoundaryToken =
      mTokenizer.AddCustomToken(mBoundary, mTokenizer.CASE_SENSITIVE);
  mBoundaryTokenWithDashes =
      mTokenizer.AddCustomToken("--"_ns + mBoundary, mTokenizer.CASE_SENSITIVE);

  return NS_OK;
}

// nsIStreamListener implementation
NS_IMETHODIMP
nsMultiMixedConv::OnDataAvailable(nsIRequest* request, nsIInputStream* inStr,
                                  uint64_t sourceOffset, uint32_t count) {
  // Failing these assertions may indicate that some of the target listeners of
  // this converter is looping the thead queue, which is harmful to how we
  // collect the raw (content) data.
  MOZ_DIAGNOSTIC_ASSERT(!mInOnDataAvailable,
                        "nsMultiMixedConv::OnDataAvailable reentered!");
  MOZ_DIAGNOSTIC_ASSERT(
      !mRawData, "There are unsent data from the previous tokenizer feed!");

  if (mInOnDataAvailable) {
    // The multipart logic is incapable of being reentered.
    return NS_ERROR_UNEXPECTED;
  }

  mozilla::AutoRestore<bool> restore(mInOnDataAvailable);
  mInOnDataAvailable = true;

  nsresult rv_feed = mTokenizer.FeedInput(inStr, count);
  // We must do this every time.  Regardless if something has failed during the
  // parsing process.  Otherwise the raw data reference would not be thrown
  // away.
  nsresult rv_send = SendData();

  return NS_FAILED(rv_send) ? rv_send : rv_feed;
}

NS_IMETHODIMP
nsMultiMixedConv::OnDataFinished(nsresult aStatus) { return NS_OK; }

NS_IMETHODIMP
nsMultiMixedConv::CheckListenerChain() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
nsMultiMixedConv::OnStopRequest(nsIRequest* request, nsresult aStatus) {
  nsresult rv;

  if (mPartChannel) {
    mPartChannel->SetIsLastPart();

    MOZ_DIAGNOSTIC_ASSERT(
        !mRawData, "There are unsent data from the previous tokenizer feed!");

    rv = mTokenizer.FinishInput();
    if (NS_SUCCEEDED(aStatus)) {
      aStatus = rv;
    }
    rv = SendData();
    if (NS_SUCCEEDED(aStatus)) {
      aStatus = rv;
    }

    (void)SendStop(aStatus);
  } else if (NS_FAILED(aStatus) && !mRequestListenerNotified) {
    // underlying data production problem. we should not be in
    // the middle of sending data. if we were, mPartChannel,
    // above, would have been non-null.

    (void)mFinalListener->OnStartRequest(request);
    (void)mFinalListener->OnStopRequest(request, aStatus);
  }

  nsCOMPtr<nsIMultiPartChannelListener> multiListener =
      do_QueryInterface(mFinalListener);
  if (multiListener) {
    multiListener->OnAfterLastPart(aStatus);
  }

  return NS_OK;
}

nsresult nsMultiMixedConv::ConsumeToken(Token const& token) {
  nsresult rv;

  switch (mParserState) {
    case PREAMBLE:
      if (token.Equals(mBoundaryTokenWithDashes)) {
        // The server first used boundary '--boundary'.  Hence, we no longer
        // accept plain 'boundary' token as a delimiter.
        mTokenizer.RemoveCustomToken(mBoundaryToken);
        mParserState = BOUNDARY_CRLF;
        break;
      }
      if (token.Equals(mBoundaryToken)) {
        // And here the opposite from the just above block...
        mTokenizer.RemoveCustomToken(mBoundaryTokenWithDashes);
        mParserState = BOUNDARY_CRLF;
        break;
      }

      // This is a preamble, just ignore it and wait for the boundary.
      break;

    case BOUNDARY_CRLF:
      if (token.Equals(Token::NewLine())) {
        mParserState = HEADER_NAME;
        mResponseHeader = HEADER_UNKNOWN;
        HeadersToDefault();
        SetHeaderTokensEnabled(true);
        break;
      }
      return NS_ERROR_CORRUPTED_CONTENT;

    case HEADER_NAME:
      SetHeaderTokensEnabled(false);
      if (token.Equals(Token::NewLine())) {
        mParserState = BODY_INIT;
        SwitchToBodyParsing();
        break;
      }
      for (uint32_t h = HEADER_CONTENT_TYPE; h < HEADER_UNKNOWN; ++h) {
        if (token.Equals(mHeaderTokens[h])) {
          mResponseHeader = static_cast<EHeader>(h);
          break;
        }
      }
      mParserState = HEADER_SEP;
      break;

    case HEADER_SEP:
      if (token.Equals(Token::Char(':'))) {
        mParserState = HEADER_VALUE;
        mResponseHeaderValue.Truncate();
        break;
      }
      if (mResponseHeader == HEADER_UNKNOWN) {
        // If the header is not of any we understand, just pass everything till
        // ':'
        break;
      }
      if (token.Equals(Token::Whitespace())) {
        // Accept only header-name traling whitespaces after known headers
        break;
      }
      return NS_ERROR_CORRUPTED_CONTENT;

    case HEADER_VALUE:
      if (token.Equals(Token::Whitespace()) && mResponseHeaderValue.IsEmpty()) {
        // Eat leading whitespaces
        break;
      }
      if (token.Equals(Token::NewLine())) {
        nsresult rv = ProcessHeader();
        if (NS_FAILED(rv)) {
          return rv;
        }
        mParserState = HEADER_NAME;
        mResponseHeader = HEADER_UNKNOWN;
        SetHeaderTokensEnabled(true);
      } else {
        mResponseHeaderValue.Append(token.Fragment());
      }
      break;

    case BODY_INIT:
      rv = SendStart();
      if (NS_FAILED(rv)) {
        return rv;
      }
      mParserState = BODY;
      [[fallthrough]];

    case BODY: {
      if (!token.Equals(mLFToken) && !token.Equals(mCRLFToken)) {
        if (token.Equals(mBoundaryTokenWithDashes) ||
            token.Equals(mBoundaryToken)) {
          // Allow CRLF to NOT be part of the boundary as well
          SwitchToControlParsing();
          mParserState = TRAIL_DASH1;
          break;
        }
        AccumulateData(token);
        break;
      }

      // After CRLF we must explicitly check for boundary.  If found,
      // that CRLF is part of the boundary and must not be send to the
      // data listener.
      Token token2;
      if (!mTokenizer.Next(token2)) {
        // Note: this will give us the CRLF token again when more data
        // or OnStopRequest arrive.  I.e. we will enter BODY case in
        // the very same state as we are now and start this block over.
        mTokenizer.NeedMoreInput();
        break;
      }
      if (token2.Equals(mBoundaryTokenWithDashes) ||
          token2.Equals(mBoundaryToken)) {
        SwitchToControlParsing();
        mParserState = TRAIL_DASH1;
        break;
      }

      AccumulateData(token);
      AccumulateData(token2);
      break;
    }

    case TRAIL_DASH1:
      if (token.Equals(Token::NewLine())) {
        rv = SendStop(NS_OK);
        if (NS_FAILED(rv)) {
          return rv;
        }
        mParserState = BOUNDARY_CRLF;
        mTokenizer.Rollback();
        break;
      }
      if (token.Equals(Token::Char('-'))) {
        mParserState = TRAIL_DASH2;
        break;
      }
      return NS_ERROR_CORRUPTED_CONTENT;

    case TRAIL_DASH2:
      if (token.Equals(Token::Char('-'))) {
        mPartChannel->SetIsLastPart();
        // SendStop calls SendData first.
        rv = SendStop(NS_OK);
        if (NS_FAILED(rv)) {
          return rv;
        }
        mParserState = EPILOGUE;
        break;
      }
      return NS_ERROR_CORRUPTED_CONTENT;

    case EPILOGUE:
      // Just ignore
      break;

    default:
      MOZ_ASSERT(false, "Missing parser state handling branch");
      break;
  }  // switch

  return NS_OK;
}

void nsMultiMixedConv::SetHeaderTokensEnabled(bool aEnable) {
  for (uint32_t h = HEADER_FIRST; h < HEADER_UNKNOWN; ++h) {
    mTokenizer.EnableCustomToken(mHeaderTokens[h], aEnable);
  }
}

void nsMultiMixedConv::SwitchToBodyParsing() {
  mTokenizer.SetTokenizingMode(Tokenizer::Mode::CUSTOM_ONLY);
  mTokenizer.EnableCustomToken(mLFToken, true);
  mTokenizer.EnableCustomToken(mCRLFToken, true);
  mTokenizer.EnableCustomToken(mBoundaryTokenWithDashes, true);
  mTokenizer.EnableCustomToken(mBoundaryToken, true);
}

void nsMultiMixedConv::SwitchToControlParsing() {
  mTokenizer.SetTokenizingMode(Tokenizer::Mode::FULL);
  mTokenizer.EnableCustomToken(mLFToken, false);
  mTokenizer.EnableCustomToken(mCRLFToken, false);
  mTokenizer.EnableCustomToken(mBoundaryTokenWithDashes, false);
  mTokenizer.EnableCustomToken(mBoundaryToken, false);
}

// nsMultiMixedConv methods
nsMultiMixedConv::nsMultiMixedConv()
    // XXX: This is a hack to bypass the raw pointer to refcounted object in
    // lambda analysis. It should be removed and replaced when the
    // IncrementalTokenizer API is improved to avoid the need for such
    // workarounds.
    //
    // This is safe because `mTokenizer` will not outlive `this`, meaning
    // that this std::bind object will be destroyed before `this` dies.
    : mTokenizer(std::bind(&nsMultiMixedConv::ConsumeToken, this,
                           std::placeholders::_1)) {}

nsresult nsMultiMixedConv::SendStart() {
  nsresult rv = NS_OK;

  nsCOMPtr<nsIStreamListener> partListener(mFinalListener);
  if (mContentType.IsEmpty()) {
    mContentType.AssignLiteral(UNKNOWN_CONTENT_TYPE);
    nsCOMPtr<nsIStreamConverterService> serv;
    serv = mozilla::components::StreamConverter::Service(&rv);
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStreamListener> converter;
      rv = serv->AsyncConvertData(UNKNOWN_CONTENT_TYPE, "*/*", mFinalListener,
                                  mContext, getter_AddRefs(converter));
      if (NS_SUCCEEDED(rv)) {
        partListener = converter;
      }
    }
  }

  // if we already have an mPartChannel, that means we never sent a Stop()
  // before starting up another "part." that would be bad.
  MOZ_ASSERT(!mPartChannel, "tisk tisk, shouldn't be overwriting a channel");

  nsPartChannel* newChannel;
  newChannel = new nsPartChannel(mChannel, mCurrentPartID, mCurrentPartID == 0,
                                 partListener);

  ++mCurrentPartID;

  if (mIsByteRangeRequest) {
    newChannel->InitializeByteRange(mByteRangeStart, mByteRangeEnd);
  }

  mTotalSent = 0;

  // Set up the new part channel...
  mPartChannel = newChannel;

  rv = mPartChannel->SetContentType(mContentType);
  if (NS_FAILED(rv)) return rv;

  rv = mPartChannel->SetContentLength(mContentLength);
  if (NS_FAILED(rv)) return rv;

  if (!mRootContentDisposition.IsEmpty()) {
    mPartChannel->SetContentDisposition(mRootContentDisposition);
  } else {
    mPartChannel->SetContentDisposition(mContentDisposition);
  }

  // Each part of a multipart/replace response can be used
  // for the top level document.  We must inform upper layers
  // about this by setting the LOAD_REPLACE flag so that certain
  // state assertions are evaluated as positive.
  nsLoadFlags loadFlags = 0;
  mPartChannel->GetLoadFlags(&loadFlags);
  loadFlags |= nsIChannel::LOAD_REPLACE;
  mPartChannel->SetLoadFlags(loadFlags);

  nsCOMPtr<nsILoadGroup> loadGroup;
  (void)mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));

  // Add the new channel to the load group (if any)
  if (loadGroup) {
    rv = loadGroup->AddRequest(mPartChannel, nullptr);
    if (NS_FAILED(rv)) return rv;
  }

  // This prevents artificial call to OnStart/StopRequest when the root
  // channel fails.  Since now it's ensured to keep with the nsIStreamListener
  // contract every time.
  mRequestListenerNotified = true;

  // Let's start off the load. NOTE: we don't forward on the channel passed
  // into our OnDataAvailable() as it's the root channel for the raw stream.
  return mPartChannel->SendOnStartRequest(mContext);
}

nsresult nsMultiMixedConv::SendStop(nsresult aStatus) {
  // Make sure we send out all accumulcated data prior call to OnStopRequest.
  // If there is no data, this is a no-op.
  nsresult rv = SendData();
  if (NS_SUCCEEDED(aStatus)) {
    aStatus = rv;
  }
  if (mPartChannel) {
    rv = mPartChannel->SendOnStopRequest(mContext, aStatus);
    // don't check for failure here, we need to remove the channel from
    // the loadgroup.

    // Remove the channel from its load group (if any)
    nsCOMPtr<nsILoadGroup> loadGroup;
    (void)mPartChannel->GetLoadGroup(getter_AddRefs(loadGroup));
    if (loadGroup) {
      (void)loadGroup->RemoveRequest(mPartChannel, mContext, aStatus);
    }
  }

  mPartChannel = nullptr;
  return rv;
}

void nsMultiMixedConv::AccumulateData(Token const& aToken) {
  if (!mRawData) {
    // This is the first read of raw data during this FeedInput loop
    // of the incremental tokenizer.  All 'raw' tokens are coming from
    // the same linear buffer, hence begining of this loop raw data
    // is begining of the first raw token.  Length of this loop raw
    // data is just sum of all 'raw' tokens we collect during this loop.
    //
    // It's ensured we flush (send to to the listener via OnDataAvailable)
    // and nullify the collected raw data right after FeedInput call.
    // Hence, the reference can't outlive the actual buffer.
    mRawData = aToken.Fragment().BeginReading();
    mRawDataLength = 0;
  }

  mRawDataLength += aToken.Fragment().Length();
}

nsresult nsMultiMixedConv::SendData() {
  nsresult rv;

  if (!mRawData) {
    return NS_OK;
  }

  nsACString::const_char_iterator rawData = mRawData;
  mRawData = nullptr;

  if (!mPartChannel) {
    return NS_ERROR_FAILURE;  // something went wrong w/ processing
  }

  if (mContentLength != UINT64_MAX) {
    // make sure that we don't send more than the mContentLength
    // XXX why? perhaps the Content-Length header was actually wrong!!
    if ((uint64_t(mRawDataLength) + mTotalSent) > mContentLength) {
      mRawDataLength = static_cast<uint32_t>(mContentLength - mTotalSent);
    }

    if (mRawDataLength == 0) return NS_OK;
  }

  uint64_t offset = mTotalSent;
  mTotalSent += mRawDataLength;

  nsCOMPtr<nsIStringInputStream> ss(
      do_CreateInstance("@mozilla.org/io/string-input-stream;1", &rv));
  if (NS_FAILED(rv)) return rv;

  rv = ss->ShareData(rawData, mRawDataLength);
  mRawData = nullptr;
  if (NS_FAILED(rv)) return rv;

  return mPartChannel->SendOnDataAvailable(mContext, ss, offset,
                                           mRawDataLength);
}

void nsMultiMixedConv::HeadersToDefault() {
  mContentLength = UINT64_MAX;
  mContentType.Truncate();
  mContentDisposition.Truncate();
  mContentSecurityPolicy.Truncate();
  mIsByteRangeRequest = false;
}

nsresult nsMultiMixedConv::ProcessHeader() {
  mozilla::Tokenizer p(mResponseHeaderValue);

  switch (mResponseHeader) {
    case HEADER_CONTENT_TYPE:
      mContentType = mResponseHeaderValue;
      mContentType.CompressWhitespace();
      break;
    case HEADER_CONTENT_LENGTH:
      p.SkipWhites();
      if (!p.ReadInteger(&mContentLength)) {
        return NS_ERROR_CORRUPTED_CONTENT;
      }
      break;
    case HEADER_CONTENT_DISPOSITION:
      mContentDisposition = mResponseHeaderValue;
      mContentDisposition.CompressWhitespace();
      break;
    case HEADER_SET_COOKIE: {
      nsCOMPtr<nsIHttpChannelInternal> httpInternal =
          do_QueryInterface(mChannel);
      mResponseHeaderValue.CompressWhitespace();
      if (!StaticPrefs::network_cookie_prevent_set_cookie_from_multipart() &&
          httpInternal) {
        DebugOnly<nsresult> rv = httpInternal->SetCookie(mResponseHeaderValue);
        MOZ_ASSERT(NS_SUCCEEDED(rv));
      }
      break;
    }
    case HEADER_RANGE:
    case HEADER_CONTENT_RANGE: {
      if (!p.CheckWord("bytes") || !p.CheckWhite()) {
        return NS_ERROR_CORRUPTED_CONTENT;
      }
      p.SkipWhites();
      if (p.CheckChar('*')) {
        mByteRangeStart = mByteRangeEnd = 0;
      } else if (!p.ReadInteger(&mByteRangeStart) || !p.CheckChar('-') ||
                 !p.ReadInteger(&mByteRangeEnd)) {
        return NS_ERROR_CORRUPTED_CONTENT;
      }
      mIsByteRangeRequest = true;
      if (mContentLength == UINT64_MAX) {
        mContentLength = uint64_t(mByteRangeEnd - mByteRangeStart + 1);
      }
      break;
    }
    case HEADER_CONTENT_SECURITY_POLICY: {
      mContentSecurityPolicy = mResponseHeaderValue;
      mContentSecurityPolicy.CompressWhitespace();
      nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
      if (httpChannel) {
        nsCString resultCSP = mRootContentSecurityPolicy;
        if (!mContentSecurityPolicy.IsEmpty()) {
          // We are updating the root channel CSP header respectively for
          // each part as: CSP-root + CSP-partN, where N is the part number.
          // Here we append current part's CSP to root CSP and reset CSP
          // header for each part.
          if (!resultCSP.IsEmpty()) {
            resultCSP.Append(";");
          }
          resultCSP.Append(mContentSecurityPolicy);
        }
        nsresult rv = httpChannel->SetResponseHeader(
            "Content-Security-Policy"_ns, resultCSP, false);
        if (NS_FAILED(rv)) {
          return NS_ERROR_CORRUPTED_CONTENT;
        }
      }
      break;
    }
    case HEADER_UNKNOWN:
      // We ignore anything else...
      break;
  }

  return NS_OK;
}

nsresult NS_NewMultiMixedConv(nsMultiMixedConv** aMultiMixedConv) {
  MOZ_ASSERT(aMultiMixedConv != nullptr, "null ptr");

  RefPtr<nsMultiMixedConv> conv = new nsMultiMixedConv();
  conv.forget(aMultiMixedConv);
  return NS_OK;
}
