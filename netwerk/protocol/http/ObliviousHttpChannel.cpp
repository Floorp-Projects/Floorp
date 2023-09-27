/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "ObliviousHttpChannel.h"

#include "BinaryHttpRequest.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsStringStream.h"

namespace mozilla::net {

NS_IMPL_ISUPPORTS(ObliviousHttpChannel, nsIChannel, nsIHttpChannel,
                  nsIObliviousHttpChannel, nsIHttpChannelInternal,
                  nsIIdentChannel, nsIRequest, nsIRequestObserver,
                  nsIStreamListener, nsIUploadChannel2, nsITimedChannel)

ObliviousHttpChannel::ObliviousHttpChannel(
    nsIURI* targetURI, const nsTArray<uint8_t>& encodedConfig,
    nsIHttpChannel* innerChannel)
    : mTargetURI(targetURI),
      mEncodedConfig(encodedConfig.Clone()),
      mInnerChannel(innerChannel),
      mInnerChannelInternal(do_QueryInterface(innerChannel)),
      mInnerChannelTimed(do_QueryInterface(innerChannel)) {
  LOG(("ObliviousHttpChannel ctor [this=%p]", this));
  MOZ_ASSERT(mInnerChannel);
  MOZ_ASSERT(mInnerChannelInternal);
  MOZ_ASSERT(mInnerChannelTimed);
}

ObliviousHttpChannel::~ObliviousHttpChannel() {
  LOG(("ObliviousHttpChannel dtor [this=%p]", this));
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ObliviousHttpChannel::GetTopLevelContentWindowId(uint64_t* aWindowId) {
  return mInnerChannel->GetTopLevelContentWindowId(aWindowId);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetTopLevelContentWindowId(uint64_t aWindowId) {
  return mInnerChannel->SetTopLevelContentWindowId(aWindowId);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetBrowserId(uint64_t* aWindowId) {
  return mInnerChannel->GetBrowserId(aWindowId);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetBrowserId(uint64_t aId) {
  return mInnerChannel->SetBrowserId(aId);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetTransferSize(uint64_t* aTransferSize) {
  return mInnerChannel->GetTransferSize(aTransferSize);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRequestSize(uint64_t* aRequestSize) {
  return mInnerChannel->GetRequestSize(aRequestSize);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetDecodedBodySize(uint64_t* aDecodedBodySize) {
  return mInnerChannel->GetDecodedBodySize(aDecodedBodySize);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRequestMethod(nsACString& aRequestMethod) {
  aRequestMethod.Assign(mMethod);
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::SetRequestMethod(const nsACString& aRequestMethod) {
  mMethod.Assign(aRequestMethod);
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetReferrerInfo(nsIReferrerInfo** aReferrerInfo) {
  return mInnerChannel->GetReferrerInfo(aReferrerInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetReferrerInfo(nsIReferrerInfo* aReferrerInfo) {
  return mInnerChannel->SetReferrerInfo(aReferrerInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetReferrerInfoWithoutClone(
    nsIReferrerInfo* aReferrerInfo) {
  return mInnerChannel->SetReferrerInfoWithoutClone(aReferrerInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRequestHeader(const nsACString& aHeader,
                                       nsACString& _retval) {
  _retval.Truncate();
  auto value = mHeaders.Lookup(aHeader);
  if (!value) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  _retval.Assign(*value);
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::SetRequestHeader(const nsACString& aHeader,
                                       const nsACString& aValue, bool aMerge) {
  mHeaders.WithEntryHandle(aHeader, [&aValue, aMerge](auto&& entry) {
    if (!entry) {
      entry.Insert(aValue);
      return;
    }
    if (!aMerge) {
      entry.Update(aValue);
      return;
    }
    nsAutoCString newValue(*entry);
    newValue.AppendLiteral(", ");
    newValue.Append(aValue);
    entry.Update(newValue);
  });
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::SetNewReferrerInfo(
    const nsACString& aUrl, nsIReferrerInfo::ReferrerPolicyIDL aPolicy,
    bool aSendReferrer) {
  return mInnerChannel->SetNewReferrerInfo(aUrl, aPolicy, aSendReferrer);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetEmptyRequestHeader(const nsACString& aHeader) {
  return SetRequestHeader(aHeader, EmptyCString(), false);
}

NS_IMETHODIMP
ObliviousHttpChannel::VisitRequestHeaders(nsIHttpHeaderVisitor* aVisitor) {
  for (auto iter = mHeaders.ConstIter(); !iter.Done(); iter.Next()) {
    nsresult rv = aVisitor->VisitHeader(iter.Key(), iter.Data());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::VisitNonDefaultRequestHeaders(
    nsIHttpHeaderVisitor* aVisitor) {
  return mInnerChannel->VisitNonDefaultRequestHeaders(aVisitor);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetAllowSTS(bool* aAllowSTS) {
  return mInnerChannel->GetAllowSTS(aAllowSTS);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetAllowSTS(bool aAllowSTS) {
  return mInnerChannel->SetAllowSTS(aAllowSTS);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRedirectionLimit(uint32_t* aRedirectionLimit) {
  return mInnerChannel->GetRedirectionLimit(aRedirectionLimit);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetRedirectionLimit(uint32_t aRedirectionLimit) {
  return mInnerChannel->SetRedirectionLimit(aRedirectionLimit);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetResponseStatus(uint32_t* aResponseStatus) {
  if (!mBinaryHttpResponse) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  uint16_t responseStatus;
  nsresult rv = mBinaryHttpResponse->GetStatus(&responseStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aResponseStatus = responseStatus;
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetResponseStatusText(nsACString& aResponseStatusText) {
  LOG(("ObliviousHttpChannel::GetResponseStatusText NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRequestSucceeded(bool* aRequestSucceeded) {
  uint32_t responseStatus;
  nsresult rv = GetResponseStatus(&responseStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aRequestSucceeded = (responseStatus / 100) == 2;
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetIsMainDocumentChannel(bool* aValue) {
  return mInnerChannel->GetIsMainDocumentChannel(aValue);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetIsMainDocumentChannel(bool aValue) {
  return mInnerChannel->SetIsMainDocumentChannel(aValue);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetResponseHeader(const nsACString& header,
                                        nsACString& _retval) {
  if (!mBinaryHttpResponse) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsTArray<nsCString> responseHeaderNames;
  nsTArray<nsCString> responseHeaderValues;
  nsresult rv = mBinaryHttpResponse->GetHeaderNames(responseHeaderNames);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mBinaryHttpResponse->GetHeaderValues(responseHeaderValues);
  if (NS_FAILED(rv)) {
    return rv;
  }
  for (size_t i = 0;
       i < responseHeaderNames.Length() && i < responseHeaderValues.Length();
       i++) {
    if (responseHeaderNames[i].EqualsIgnoreCase(header)) {
      _retval.Assign(responseHeaderValues[i]);
      return NS_OK;
    }
  }
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
ObliviousHttpChannel::SetResponseHeader(const nsACString& header,
                                        const nsACString& value, bool merge) {
  LOG(("ObliviousHttpChannel::SetResponseHeader NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::VisitResponseHeaders(nsIHttpHeaderVisitor* aVisitor) {
  if (!mBinaryHttpResponse) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  nsTArray<nsCString> responseHeaderNames;
  nsTArray<nsCString> responseHeaderValues;
  nsresult rv = mBinaryHttpResponse->GetHeaderNames(responseHeaderNames);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mBinaryHttpResponse->GetHeaderValues(responseHeaderValues);
  if (NS_FAILED(rv)) {
    return rv;
  }
  for (size_t i = 0;
       i < responseHeaderNames.Length() && i < responseHeaderValues.Length();
       i++) {
    rv = aVisitor->VisitHeader(responseHeaderNames[i], responseHeaderValues[i]);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  return rv;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetOriginalResponseHeader(
    const nsACString& header, nsIHttpHeaderVisitor* aVisitor) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::VisitOriginalResponseHeaders(
    nsIHttpHeaderVisitor* aVisitor) {
  LOG(
      ("ObliviousHttpChannel::VisitOriginalResponseHeaders NOT IMPLEMENTED "
       "[this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::ShouldStripRequestBodyHeader(const nsACString& aMethod,
                                                   bool* aResult) {
  LOG(
      ("ObliviousHttpChannel::ShouldStripRequestBodyHeader NOT IMPLEMENTED "
       "[this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::IsNoStoreResponse(bool* _retval) {
  LOG(("ObliviousHttpChannel::IsNoStoreResponse NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::IsNoCacheResponse(bool* _retval) {
  LOG(("ObliviousHttpChannel::IsNoCacheResponse NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::IsPrivateResponse(bool* _retval) {
  LOG(("ObliviousHttpChannel::IsPrivateResponse NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::RedirectTo(nsIURI* aNewURI) {
  LOG(("ObliviousHttpChannel::RedirectTo NOT IMPLEMENTED [this=%p]", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::UpgradeToSecure() {
  LOG(("ObliviousHttpChannel::UpgradeToSecure NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetRequestContextID(uint64_t* _retval) {
  return mInnerChannel->GetRequestContextID(_retval);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetRequestContextID(uint64_t rcID) {
  return mInnerChannel->SetRequestContextID(rcID);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetProtocolVersion(nsACString& aProtocolVersion) {
  return mInnerChannel->GetProtocolVersion(aProtocolVersion);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetEncodedBodySize(uint64_t* aEncodedBodySize) {
  LOG(("ObliviousHttpChannel::GetEncodedBodySize NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::LogBlockedCORSRequest(const nsAString& aMessage,
                                            const nsACString& aCategory,
                                            bool aIsWarning) {
  return mInnerChannel->LogBlockedCORSRequest(aMessage, aCategory, aIsWarning);
}

NS_IMETHODIMP
ObliviousHttpChannel::LogMimeTypeMismatch(const nsACString& aMessageName,
                                          bool aWarning, const nsAString& aURL,
                                          const nsAString& aContentType) {
  return mInnerChannel->LogMimeTypeMismatch(aMessageName, aWarning, aURL,
                                            aContentType);
}

void ObliviousHttpChannel::SetSource(
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource) {
  LOG(("ObliviousHttpChannel::SetSource NOT IMPLEMENTED [this=%p]", this));
  // NS_ERROR_NOT_IMPLEMENTED
}

void ObliviousHttpChannel::SetConnectionInfo(nsHttpConnectionInfo* aCi) {
  if (mInnerChannelInternal) {
    mInnerChannelInternal->SetConnectionInfo(aCi);
  }
}

void ObliviousHttpChannel::DoDiagnosticAssertWhenOnStopNotCalledOnDestroy() {
  if (mInnerChannelInternal) {
    mInnerChannelInternal->DoDiagnosticAssertWhenOnStopNotCalledOnDestroy();
  }
}

void ObliviousHttpChannel::DisableAltDataCache() {
  if (mInnerChannelInternal) {
    mInnerChannelInternal->DisableAltDataCache();
  }
}

void ObliviousHttpChannel::SetAltDataForChild(bool aIsForChild) {
  if (mInnerChannelInternal) {
    mInnerChannelInternal->SetAltDataForChild(aIsForChild);
  }
}

void ObliviousHttpChannel::SetCorsPreflightParameters(
    nsTArray<nsTString<char>> const& aUnsafeHeaders,
    bool aShouldStripRequestBodyHeader) {
  if (mInnerChannelInternal) {
    mInnerChannelInternal->SetCorsPreflightParameters(
        aUnsafeHeaders, aShouldStripRequestBodyHeader);
  }
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ObliviousHttpChannel::GetOriginalURI(nsIURI** aOriginalURI) {
  return mInnerChannel->GetOriginalURI(aOriginalURI);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetOriginalURI(nsIURI* aOriginalURI) {
  return mInnerChannel->SetOriginalURI(aOriginalURI);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetURI(nsIURI** aURI) {
  *aURI = do_AddRef(mTargetURI).take();
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetOwner(nsISupports** aOwner) {
  return mInnerChannel->GetOwner(aOwner);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetOwner(nsISupports* aOwner) {
  return mInnerChannel->SetOwner(aOwner);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aNotificationCallbacks) {
  return mInnerChannel->GetNotificationCallbacks(aNotificationCallbacks);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aNotificationCallbacks) {
  return mInnerChannel->SetNotificationCallbacks(aNotificationCallbacks);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  return mInnerChannel->GetSecurityInfo(aSecurityInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentType(nsACString& aContentType) {
  return GetResponseHeader("content-type"_ns, aContentType);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetContentType(const nsACString& aContentType) {
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentCharset(nsACString& aContentCharset) {
  return mInnerChannel->GetContentCharset(aContentCharset);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetContentCharset(const nsACString& aContentCharset) {
  return mInnerChannel->SetContentCharset(aContentCharset);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentLength(int64_t* aContentLength) {
  return mInnerChannel->GetContentLength(aContentLength);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetContentLength(int64_t aContentLength) {
  return mInnerChannel->SetContentLength(aContentLength);
}

NS_IMETHODIMP
ObliviousHttpChannel::Open(nsIInputStream** aStream) {
  LOG(("ObliviousHttpChannel::Open NOT IMPLEMENTED [this=%p]", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ObliviousHttpChannel::AsyncOpen(nsIStreamListener* aListener) {
  LOG(("ObliviousHttpChannel::AsyncOpen [this=%p, listener=%p]", this,
       aListener));
  mStreamListener = aListener;
  nsresult rv = mInnerChannel->SetRequestMethod("POST"_ns);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = mInnerChannel->SetRequestHeader("Content-Type"_ns,
                                       "message/ohttp-req"_ns, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString scheme;
  if (!mTargetURI) {
    return NS_ERROR_NULL_POINTER;
  }
  rv = mTargetURI->GetScheme(scheme);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString authority;
  rv = mTargetURI->GetHostPort(authority);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsAutoCString path;
  rv = mTargetURI->GetPathQueryRef(path);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsTArray<nsCString> headerNames;
  nsTArray<nsCString> headerValues;
  for (auto iter = mHeaders.ConstIter(); !iter.Done(); iter.Next()) {
    headerNames.AppendElement(iter.Key());
    headerValues.AppendElement(iter.Data());
  }
  if (!mContentType.IsEmpty() && !headerNames.Contains("Content-Type")) {
    headerNames.AppendElement("Content-Type"_ns);
    headerValues.AppendElement(mContentType);
  }
  nsCOMPtr<nsIBinaryHttp> bhttp(
      do_GetService("@mozilla.org/network/binary-http;1"));
  nsCOMPtr<nsIBinaryHttpRequest> bhttpRequest(new BinaryHttpRequest(
      mMethod, scheme, authority, path, std::move(headerNames),
      std::move(headerValues), std::move(mContent)));
  nsTArray<uint8_t> encodedRequest;
  rv = bhttp->EncodeRequest(bhttpRequest, encodedRequest);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIObliviousHttp> obliviousHttp(
      do_GetService("@mozilla.org/network/oblivious-http;1"));
  if (!obliviousHttp) {
    return NS_ERROR_FAILURE;
  }
  rv = obliviousHttp->EncapsulateRequest(mEncodedConfig, encodedRequest,
                                         getter_AddRefs(mEncapsulatedRequest));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsTArray<uint8_t> encRequest;
  rv = mEncapsulatedRequest->GetEncRequest(encRequest);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIUploadChannel2> uploadChannel(do_QueryInterface(mInnerChannel));
  if (!uploadChannel) {
    return NS_ERROR_UNEXPECTED;
  }
  nsCOMPtr<nsIInputStream> uploadStream;
  uint32_t streamLength = encRequest.Length();
  rv = NS_NewByteInputStream(getter_AddRefs(uploadStream),
                             std::move(encRequest));
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = uploadChannel->ExplicitSetUploadStream(
      uploadStream, "message/ohttp-req"_ns, streamLength, "POST"_ns, false);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return mInnerChannel->AsyncOpen(this);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetCanceled(bool* aCanceled) {
  return mInnerChannel->GetCanceled(aCanceled);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentDisposition(uint32_t* aContentDisposition) {
  return mInnerChannel->GetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetContentDisposition(uint32_t aContentDisposition) {
  return mInnerChannel->SetContentDisposition(aContentDisposition);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return mInnerChannel->GetContentDispositionFilename(
      aContentDispositionFilename);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return mInnerChannel->SetContentDispositionFilename(
      aContentDispositionFilename);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return mInnerChannel->GetContentDispositionHeader(aContentDispositionHeader);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  return mInnerChannel->GetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  return mInnerChannel->SetLoadInfo(aLoadInfo);
}

NS_IMETHODIMP
ObliviousHttpChannel::GetIsDocument(bool* aIsDocument) {
  return mInnerChannel->GetIsDocument(aIsDocument);
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ObliviousHttpChannel::OnDataAvailable(nsIRequest* aRequest,
                                      nsIInputStream* aStream, uint64_t aOffset,
                                      uint32_t aCount) {
  LOG(
      ("ObliviousHttpChannel::OnDataAvailable [this=%p, request=%p, stream=%p, "
       "offset=%" PRIu64 ", count=%u]",
       this, aRequest, aStream, aOffset, aCount));
  MOZ_ASSERT(aOffset == mRawResponse.Length());
  size_t oldLength = mRawResponse.Length();
  size_t newLength = oldLength + aCount;
  if (newLength < oldLength) {  // i.e., overflow
    return NS_ERROR_FAILURE;
  }
  mRawResponse.SetCapacity(newLength);
  mRawResponse.SetLengthAndRetainStorage(newLength);
  void* dest = mRawResponse.Elements() + oldLength;
  uint64_t written = 0;
  nsresult rv = NS_ReadInputStreamToBuffer(aStream, &dest, aCount, &written);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (written != aCount) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ObliviousHttpChannel::OnStartRequest(nsIRequest* aRequest) {
  LOG(("ObliviousHttpChannel::OnStartRequest [this=%p, request=%p]", this,
       aRequest));
  return NS_OK;
}

nsresult ObliviousHttpChannel::ProcessOnStopRequest() {
  if (mRawResponse.IsEmpty()) {
    return NS_OK;
  }
  nsCOMPtr<nsIObliviousHttp> obliviousHttp(
      do_GetService("@mozilla.org/network/oblivious-http;1"));
  if (!obliviousHttp) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIObliviousHttpClientResponse> response;
  nsresult rv = mEncapsulatedRequest->GetResponse(getter_AddRefs(response));
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsTArray<uint8_t> decapsulated;
  rv = response->Decapsulate(mRawResponse, decapsulated);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsCOMPtr<nsIBinaryHttp> bhttp(
      do_GetService("@mozilla.org/network/binary-http;1"));
  if (!bhttp) {
    return NS_ERROR_FAILURE;
  }
  return bhttp->DecodeResponse(decapsulated,
                               getter_AddRefs(mBinaryHttpResponse));
}

void ObliviousHttpChannel::EmitOnDataAvailable() {
  if (!mBinaryHttpResponse) {
    return;
  }
  nsTArray<uint8_t> content;
  nsresult rv = mBinaryHttpResponse->GetContent(content);
  if (NS_FAILED(rv)) {
    return;
  }
  if (content.IsEmpty()) {
    return;
  }
  if (content.Length() > std::numeric_limits<uint32_t>::max()) {
    return;
  }
  uint32_t contentLength = (uint32_t)content.Length();
  nsCOMPtr<nsIInputStream> contentStream;
  rv = NS_NewByteInputStream(getter_AddRefs(contentStream), std::move(content));
  if (NS_FAILED(rv)) {
    return;
  }
  rv = mStreamListener->OnDataAvailable(this, contentStream, 0, contentLength);
  Unused << rv;
}

NS_IMETHODIMP
ObliviousHttpChannel::OnStopRequest(nsIRequest* aRequest,
                                    nsresult aStatusCode) {
  LOG(("ObliviousHttpChannel::OnStopRequest [this=%p, request=%p, status=%u]",
       this, aRequest, (uint32_t)aStatusCode));

  auto releaseStreamListener = MakeScopeExit(
      [self = RefPtr{this}]() mutable { self->mStreamListener = nullptr; });

  if (NS_SUCCEEDED(aStatusCode)) {
    bool requestSucceeded;
    nsresult rv = mInnerChannel->GetRequestSucceeded(&requestSucceeded);
    if (NS_SUCCEEDED(rv) && requestSucceeded) {
      aStatusCode = ProcessOnStopRequest();
    }
  }
  Unused << mStreamListener->OnStartRequest(this);
  if (NS_SUCCEEDED(aStatusCode)) {
    EmitOnDataAvailable();
  }
  Unused << mStreamListener->OnStopRequest(this, aStatusCode);

  return NS_OK;
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIObliviousHttpChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ObliviousHttpChannel::GetRelayChannel(nsIHttpChannel** aChannel) {
  if (!aChannel) {
    return NS_ERROR_INVALID_ARG;
  }
  *aChannel = do_AddRef(mInnerChannel).take();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ObliviousHttpChannel::nsIUploadChannel2
//-----------------------------------------------------------------------------

NS_IMETHODIMP ObliviousHttpChannel::ExplicitSetUploadStream(
    nsIInputStream* aStream, const nsACString& aContentType,
    int64_t aContentLength, const nsACString& aMethod, bool aStreamHasHeaders) {
  // This function should only be called before AsyncOpen.
  if (mStreamListener) {
    return NS_ERROR_IN_PROGRESS;
  }
  if (aMethod != "POST"_ns || aStreamHasHeaders) {
    return NS_ERROR_INVALID_ARG;
  }
  mMethod.Assign(aMethod);
  uint64_t available;
  if (aContentLength < 0) {
    nsresult rv = aStream->Available(&available);
    if (NS_FAILED(rv)) {
      return rv;
    }
  } else {
    available = aContentLength;
  }
  if (available > std::numeric_limits<int64_t>::max()) {
    return NS_ERROR_FAILURE;
  }
  mContent.SetCapacity(available);
  mContent.SetLengthAndRetainStorage(available);
  void* dest = mContent.Elements();
  uint64_t written = 0;
  nsresult rv =
      NS_ReadInputStreamToBuffer(aStream, &dest, (int64_t)available, &written);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (written != available) {
    return NS_ERROR_FAILURE;
  }
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP ObliviousHttpChannel::GetUploadStreamHasHeaders(
    bool* aUploadStreamHasHeaders) {
  *aUploadStreamHasHeaders = false;
  return NS_OK;
}

NS_IMETHODIMP ObliviousHttpChannel::CloneUploadStream(
    int64_t* aContentLength, nsIInputStream** _retval) {
  LOG(("ObliviousHttpChannel::CloneUploadStream NOT IMPLEMENTED [this=%p]",
       this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ObliviousHttpChannel::SetClassicScriptHintCharset(
    const nsAString& aClassicScriptHintCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ObliviousHttpChannel::GetClassicScriptHintCharset(
    nsAString& aClassicScriptHintCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ObliviousHttpChannel::SetDocumentCharacterSet(
    const nsAString& aDocumentCharacterSet) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP ObliviousHttpChannel::GetDocumentCharacterSet(
    nsAString& aDocumenharacterSet) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace mozilla::net
